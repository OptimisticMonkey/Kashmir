// Shadow caster mesh shader — re-renders meshlet clusters from the sun's
// perspective. Shares the GPUMeshShaderPushConstants layout with mesh.mesh
// so both passes emit pixel-equivalent positions (avoids shadow acne when
// the main pass samples the shadow map).
//
// Shadow pass intentionally has no task shader: silhouettes that the camera
// pass skips via cone-culling still need to cast shadows from the sun's
// direction, so we dispatch one mesh workgroup per (meshlet, instance) here.

import common;

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

struct PushConstants
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
[[vk::push_constant]] PushConstants pc;

struct VSOut
{
    float4 sv_position : SV_Position;
};

static const uint kMaxVerts = 64;
static const uint kMaxTris  = 124;
static const uint kGroupSize = 32;

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(kGroupSize, 1, 1)]
void main(
    uint3 gid : SV_GroupID,
    uint  tid : SV_GroupThreadID,
    out vertices VSOut verts[kMaxVerts],
    out indices  uint3 tris [kMaxTris])
{
    uint meshletIdx  = gid.x;
    uint instanceIdx = gid.y;

    Meshlet  m         = pc.meshlets[meshletIdx];
    float4x4 instXform = pc.instanceTransformBuffer[instanceIdx].transform;

    SetMeshOutputCounts(m.vertexCount, m.triangleCount);

    for (uint i = tid; i < m.vertexCount; i += kGroupSize)
    {
        uint vid = pc.meshletVertices[m.vertexOffset + i];
        Vertex v = pc.vertexBuffer[vid];

        float4 worldPos = mul(instXform, float4(v.position, 1.0));
        verts[i].sv_position = mul(sceneData.lightViewProj, worldPos);
    }

    for (uint t = tid; t < m.triangleCount; t += kGroupSize)
    {
        uint packed = pc.meshletTriangles[m.triangleOffset + t];
        tris[t] = uint3(packed & 0xffu, (packed >> 8) & 0xffu, (packed >> 16) & 0xffu);
    }
}
