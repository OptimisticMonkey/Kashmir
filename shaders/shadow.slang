// Shadow caster pass. Re-renders OpaqueSurfaces from the sun's perspective using
// the same per-draw buffer-device-address inputs as the main material. Combines:
//   vertexMain   -> shadow.vert.spv  (classic vertex path)
//   fragmentMain -> shadow.frag.spv  (empty; depth-only, shared by both paths)
//   meshMain     -> shadow.mesh.spv  (mesh-shader path; no task shader)
// Each stage is compiled to its own SPIR-V module (see CMakeLists.txt).

import common;

// ===========================================================================
// Vertex stage  ->  shadow.vert.spv
// ===========================================================================
struct PushConstants
{
    float4x4           renderMatrix;
    Vertex*            vertexBuffer;
    InstanceTransform* instanceTransformBuffer;
};
[[vk::push_constant]] PushConstants pc;

[shader("vertex")]
float4 vertexMain(uint vid : SV_VertexID, uint iid : SV_InstanceID) : SV_Position
{
    Vertex v = pc.vertexBuffer[vid];
    float4x4 instanceXform = pc.instanceTransformBuffer[iid].transform;
    float4 worldPos = mul(instanceXform, float4(v.position, 1.0));
    return mul(sceneData.lightViewProj, worldPos);
}

// ===========================================================================
// Fragment stage  ->  shadow.frag.spv   (shared by both paths)
// Depth is written automatically; nothing else is emitted.
// ===========================================================================
[shader("fragment")]
void fragmentMain()
{
}

// ===========================================================================
// Mesh stage  ->  shadow.mesh.spv
// Re-renders meshlet clusters from the sun's perspective. Shares the
// GPUMeshShaderPushConstants layout with mesh.slang so both passes emit
// pixel-equivalent positions (avoids shadow acne when the main pass samples
// the shadow map).
//
// Shadow pass intentionally has no task shader: silhouettes the camera pass
// skips via cone-culling still need to cast shadows, so we dispatch one mesh
// workgroup per (meshlet, instance) here.
// ===========================================================================
struct Meshlet
{
    uint vertexOffset;
    uint triangleOffset;
    uint vertexCount;
    uint triangleCount;
};

struct MeshletBounds
{
    float4 centerRadius;
    float4 coneApex;
    float4 coneAxisCutoff;
};

struct MeshPushConstants
{
    float4x4           worldMatrix;
    Vertex*            vertexBuffer;
    InstanceTransform* instanceTransformBuffer;
    Meshlet*           meshlets;
    uint*              meshletVertices;
    uint*              meshletTriangles;
    MeshletBounds*     meshletBounds;
    uint               meshletCount;
    uint               instanceCount;
};
[[vk::push_constant]] MeshPushConstants pcMesh;

struct MeshVertex
{
    float4 sv_position : SV_Position;
};

static const uint kMaxVerts  = 64;
static const uint kMaxTris   = 124;
static const uint kGroupSize = 32;

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(kGroupSize, 1, 1)]
void meshMain(
    uint3 gid : SV_GroupID,
    uint  tid : SV_GroupThreadID,
    out vertices MeshVertex verts[kMaxVerts],
    out indices  uint3      tris [kMaxTris])
{
    uint meshletIdx  = gid.x;
    uint instanceIdx = gid.y;

    Meshlet  m         = pcMesh.meshlets[meshletIdx];
    float4x4 instXform = pcMesh.instanceTransformBuffer[instanceIdx].transform;

    SetMeshOutputCounts(m.vertexCount, m.triangleCount);

    for (uint i = tid; i < m.vertexCount; i += kGroupSize)
    {
        uint vid = pcMesh.meshletVertices[m.vertexOffset + i];
        Vertex v = pcMesh.vertexBuffer[vid];

        float4 worldPos = mul(instXform, float4(v.position, 1.0));
        verts[i].sv_position = mul(sceneData.lightViewProj, worldPos);
    }

    for (uint t = tid; t < m.triangleCount; t += kGroupSize)
    {
        uint packed = pcMesh.meshletTriangles[m.triangleOffset + t];
        tris[t] = uint3(packed & 0xffu, (packed >> 8) & 0xffu, (packed >> 16) & 0xffu);
    }
}
