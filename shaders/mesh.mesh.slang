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

// Hash a meshlet index into a deterministic RGB triple so cluster boundaries
// are visible in the debug view. Same input → same color frame-to-frame.
float3 hashMeshletColor(uint id)
{
    uint h = id;
    h = (h ^ 61u) ^ (h >> 16);
    h *= 9u;
    h = h ^ (h >> 4);
    h *= 0x27d4eb2du;
    h = h ^ (h >> 15);
    return float3((h        & 0xffu) / 255.0,
                  ((h >>  8) & 0xffu) / 255.0,
                  ((h >> 16) & 0xffu) / 255.0);
}

struct VSOut
{
    float4 sv_position : SV_Position;
    float3 normal      : NORMAL;
    float3 color       : COLOR;
    float2 uv          : TEXCOORD0;
    float3 worldPos    : TEXCOORD1;
    float4 shadowCoord : TEXCOORD2;
};

static const uint kMaxVerts  = 64;
static const uint kMaxTris   = 124;
static const uint kGroupSize = 32;

// Mirrors the Payload in shaders/mesh.task.slang. The task shader broadcasts
// instanceIdx and a packed list of survivor meshletIds[] to each launched
// mesh workgroup; gid.x indexes into that survivor list.
struct Payload
{
    uint instanceIdx;
    uint meshletIds[kGroupSize];
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(kGroupSize, 1, 1)]
void main(
    uint3 gid : SV_GroupID,
    uint  tid : SV_GroupThreadID,
    in payload Payload payload,
    out vertices VSOut verts[kMaxVerts],
    out indices  uint3 tris [kMaxTris])
{
    uint meshletIdx  = payload.meshletIds[gid.x];
    uint instanceIdx = payload.instanceIdx;

    Meshlet  m         = pc.meshlets[meshletIdx];
    float4x4 instXform = pc.instanceTransformBuffer[instanceIdx].transform;

    SetMeshOutputCounts(m.vertexCount, m.triangleCount);

    for (uint i = tid; i < m.vertexCount; i += kGroupSize)
    {
        uint vid = pc.meshletVertices[m.vertexOffset + i];
        Vertex v = pc.vertexBuffer[vid];

        float4 modelPos = float4(v.position, 1.0);
        float4 worldPos = mul(instXform, modelPos);
        float4 clipPos  = mul(sceneData.viewproj, worldPos);

        VSOut o;
        o.sv_position = clipPos;
        o.normal      = normalize(mul((float3x3)instXform, v.normal));
        o.color       = v.color.xyz * materialData.colorFactors.xyz;
        o.uv          = float2(v.uv_x, v.uv_y);
        o.worldPos    = worldPos.xyz;
        o.shadowCoord = mul(sceneData.lightViewProj, worldPos);
        if (sceneData.debugParams.x > 0.5)
        {
            o.color = hashMeshletColor(meshletIdx);
        }
        verts[i] = o;
    }

    for (uint t = tid; t < m.triangleCount; t += kGroupSize)
    {
        uint packed = pc.meshletTriangles[m.triangleOffset + t];
        tris[t] = uint3(packed & 0xffu, (packed >> 8) & 0xffu, (packed >> 16) & 0xffu);
    }
}
