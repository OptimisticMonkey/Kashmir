// Task (amplification) shader for per-meshlet culling.
//
// Each task workgroup processes up to kGroupSize meshlets (one per thread),
// rejects clusters that are either fully outside the camera frustum or fully
// back-facing (normal-cone test), and dispatches a single mesh workgroup per
// survivor via the task payload. Survivor compaction uses wave intrinsics —
// kGroupSize is chosen to fit in a single subgroup on both Wave32 (NVIDIA /
// AMD-RDNA-Wave32) and Wave64 (AMD GCN / RDNA-Wave64) hardware.
//
// The payload is shared with shaders/mesh.mesh.slang: instanceIdx is broadcast
// to every emitted mesh workgroup, and meshletIds[gid.x] gives the surviving
// meshlet that mesh workgroup should render.

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
    float4 centerRadius;   // xyz = center, w = radius   (mesh-local space)
    float4 coneApex;       // xyz = apex,   w = pad
    float4 coneAxisCutoff; // xyz = axis,   w = cos(angle/2)
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

static const uint kGroupSize = 32;

struct Payload
{
    uint instanceIdx;
    uint meshletIds[kGroupSize];
};

groupshared Payload sPayload;

// Six world-space frustum planes extracted from viewproj (Gribb–Hartmann).
// Returns true when the sphere is fully outside at least one plane.
bool sphereOutsideFrustum(float3 c, float r)
{
    float4x4 vp = sceneData.viewproj;
    // Slang/HLSL matrices indexed by [row]: rows of viewproj in world->clip transform.
    float4 r0 = float4(vp[0][0], vp[0][1], vp[0][2], vp[0][3]);
    float4 r1 = float4(vp[1][0], vp[1][1], vp[1][2], vp[1][3]);
    float4 r2 = float4(vp[2][0], vp[2][1], vp[2][2], vp[2][3]);
    float4 r3 = float4(vp[3][0], vp[3][1], vp[3][2], vp[3][3]);

    float4 planes[6];
    planes[0] = r3 + r0; // left   (x >= -w)
    planes[1] = r3 - r0; // right  (x <=  w)
    planes[2] = r3 + r1; // bottom (y >= -w)
    planes[3] = r3 - r1; // top    (y <=  w)
    planes[4] = r2;      // near   (z >=  0) — Vulkan [0,1] depth
    planes[5] = r3 - r2; // far    (z <=  w)

    [unroll]
    for (uint i = 0; i < 6; ++i)
    {
        float3 n   = planes[i].xyz;
        float  len = length(n);
        if (len <= 1e-6) continue;
        float signedDist = (dot(n, c) + planes[i].w) / len;
        if (signedDist + r < 0.0) return true;
    }
    return false;
}

// Apex-free normal cone test for perspective cameras (Real-Time Rendering 19.3):
//   dot(center - cam, axis) >= cutoff * length(center - cam) + radius
// → entire cluster back-facing. Cutoffs of >=1 disable the test (degenerate cone).
bool coneBackfacing(float3 worldCenter, float worldRadius, float3 worldAxis, float coneCutoff, float3 camPos)
{
    if (coneCutoff >= 1.0) return false;
    float3 toCenter = worldCenter - camPos;
    float  dist     = length(toCenter);
    return dot(toCenter, worldAxis) >= coneCutoff * dist + worldRadius;
}

[shader("amplification")]
[numthreads(kGroupSize, 1, 1)]
void main(
    uint3 gid : SV_GroupID,
    uint  tid : SV_GroupThreadID)
{
    uint meshletIdx  = gid.x * kGroupSize + tid;
    uint instanceIdx = gid.y;

    bool visible = (meshletIdx < pc.meshletCount);
    if (visible)
    {
        float4x4 instXform = pc.instanceTransformBuffer[instanceIdx].transform;
        MeshletBounds b    = pc.meshletBounds[meshletIdx];

        // Bring the bounding sphere into world space. We pull a scalar scale out
        // of the instance matrix (max column length) — fine for the
        // uniform/near-uniform scales the Suzanne spawner produces; would need
        // per-component handling for general non-uniform scale.
        float3 worldCenter = mul(instXform, float4(b.centerRadius.xyz, 1.0)).xyz;
        float  sx = length(float3(instXform[0][0], instXform[1][0], instXform[2][0]));
        float  sy = length(float3(instXform[0][1], instXform[1][1], instXform[2][1]));
        float  sz = length(float3(instXform[0][2], instXform[1][2], instXform[2][2]));
        float  scale = max(sx, max(sy, sz));
        float  worldRadius = b.centerRadius.w * scale;

        if (sphereOutsideFrustum(worldCenter, worldRadius))
        {
            visible = false;
        }
        else
        {
            float3 worldAxis = normalize(mul((float3x3)instXform, b.coneAxisCutoff.xyz));
            if (coneBackfacing(worldCenter, worldRadius, worldAxis, b.coneAxisCutoff.w, sceneData.cameraPos.xyz))
            {
                visible = false;
            }
        }
    }

    // Compact survivors into the payload using wave ops. kGroupSize == 32 fits
    // in one wave on every target so this stays a single subgroup-prefix.
    uint laneIdx     = WavePrefixCountBits(visible);
    uint surviveCount = WaveActiveCountBits(visible);
    if (visible)
    {
        sPayload.meshletIds[laneIdx] = meshletIdx;
    }
    if (tid == 0)
    {
        sPayload.instanceIdx = instanceIdx;
    }
    GroupMemoryBarrierWithGroupSync();

    DispatchMesh(surviveCount, 1, 1, sPayload);
}
