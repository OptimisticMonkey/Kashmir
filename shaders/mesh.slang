// Opaque metallic-roughness material for GLTF surfaces. Combines every stage of
// the material into one module:
//   vertexMain   -> mesh.vert.spv   (classic vertex path)
//   fragmentMain -> mesh.frag.spv   (shared by both paths)
//   taskMain     -> mesh.task.spv   (per-meshlet culling, mesh-shader path)
//   meshMain     -> mesh.mesh.spv   (meshlet rasterization, mesh-shader path)
// Each stage is compiled as its own SPIR-V module (see CMakeLists.txt), so only
// the push-constant block actually referenced by an entry point is emitted.

import common;

// ---------------------------------------------------------------------------
// Shared interpolants — written by vertexMain / meshMain, read by fragmentMain.
// ---------------------------------------------------------------------------
struct Varyings
{
    float4 sv_position : SV_Position;
    float3 normal      : NORMAL;
    float3 color       : COLOR;
    float2 uv          : TEXCOORD0;
    float3 worldPos    : TEXCOORD1;
    float4 shadowCoord : TEXCOORD2;
};

// ===========================================================================
// Vertex stage  ->  mesh.vert.spv
// ===========================================================================
struct PushConstants
{
    float4x4           renderMatrix;
    Vertex*            vertexBuffer;
    InstanceTransform* instanceTransformBuffer;
};
[[vk::push_constant]] PushConstants pc;

[shader("vertex")]
Varyings vertexMain(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    Vertex v = pc.vertexBuffer[vid];
    float4x4 instanceXform = pc.instanceTransformBuffer[iid].transform;

    float4 modelPos = float4(v.position, 1.0);
    float4 worldPos = mul(instanceXform, modelPos);

    Varyings o;
    o.sv_position = mul(sceneData.viewproj, worldPos);
    o.normal      = normalize(mul((float3x3)instanceXform, v.normal));
    o.color       = v.color.xyz * materialData.colorFactors.xyz;
    o.uv          = float2(v.uv_x, v.uv_y);
    o.worldPos    = worldPos.xyz;
    o.shadowCoord = mul(sceneData.lightViewProj, worldPos);
    return o;
}

// ===========================================================================
// Mesh-shader path: task + mesh stages
//   ->  mesh.task.spv / mesh.mesh.spv
// Uses a distinct, larger push-constant block (GPUMeshShaderPushConstants).
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
    float4 centerRadius;   // xyz = center, w = radius   (mesh-local space)
    float4 coneApex;       // xyz = apex,   w = pad
    float4 coneAxisCutoff; // xyz = axis,   w = cos(angle/2)
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

static const uint kMaxVerts  = 64;
static const uint kMaxTris   = 124;
static const uint kGroupSize = 32;

// Shared task->mesh payload: the task shader broadcasts instanceIdx and a packed
// list of survivor meshletIds[] to each launched mesh workgroup; gid.x indexes
// into that survivor list.
struct Payload
{
    uint instanceIdx;
    uint meshletIds[kGroupSize];
};

// --- Task (amplification) stage --------------------------------------------
//
// Each task workgroup processes up to kGroupSize meshlets (one per thread),
// rejects clusters that are either fully outside the camera frustum or fully
// back-facing (normal-cone test), and dispatches a single mesh workgroup per
// survivor via the task payload. Survivor compaction uses wave intrinsics —
// kGroupSize is chosen to fit in a single subgroup on both Wave32 and Wave64.

groupshared Payload sPayload;

// Six world-space frustum planes extracted from viewproj (Gribb-Hartmann).
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
void taskMain(
    uint3 gid : SV_GroupID,
    uint  tid : SV_GroupThreadID)
{
    uint meshletIdx  = gid.x * kGroupSize + tid;
    uint instanceIdx = gid.y;

    bool visible = (meshletIdx < pcMesh.meshletCount);
    if (visible)
    {
        float4x4 instXform = pcMesh.instanceTransformBuffer[instanceIdx].transform;
        MeshletBounds b    = pcMesh.meshletBounds[meshletIdx];

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

// --- Mesh stage ------------------------------------------------------------

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

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(kGroupSize, 1, 1)]
void meshMain(
    uint3 gid : SV_GroupID,
    uint  tid : SV_GroupThreadID,
    in payload Payload payload,
    out vertices Varyings verts[kMaxVerts],
    out indices  uint3    tris [kMaxTris])
{
    uint meshletIdx  = payload.meshletIds[gid.x];
    uint instanceIdx = payload.instanceIdx;

    Meshlet  m         = pcMesh.meshlets[meshletIdx];
    float4x4 instXform = pcMesh.instanceTransformBuffer[instanceIdx].transform;

    SetMeshOutputCounts(m.vertexCount, m.triangleCount);

    for (uint i = tid; i < m.vertexCount; i += kGroupSize)
    {
        uint vid = pcMesh.meshletVertices[m.vertexOffset + i];
        Vertex v = pcMesh.vertexBuffer[vid];

        float4 modelPos = float4(v.position, 1.0);
        float4 worldPos = mul(instXform, modelPos);
        float4 clipPos  = mul(sceneData.viewproj, worldPos);

        Varyings o;
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
        uint packed = pcMesh.meshletTriangles[m.triangleOffset + t];
        tris[t] = uint3(packed & 0xffu, (packed >> 8) & 0xffu, (packed >> 16) & 0xffu);
    }
}

// ===========================================================================
// Fragment stage  ->  mesh.frag.spv   (shared by the vertex and mesh paths)
// ===========================================================================
//
// Cluster-color debug flags are read from sceneData.debugParams (UBO) — NOT
// push constants — because the regular (non-mesh-shader) opaque pipeline's
// push range is VERTEX-only and 80 bytes, which would leave a fragment-stage
// read of pc.debugFlags out-of-range and trigger undefined behavior.

static const float PI = 3.14159265359;

// GGX / Trowbridge-Reitz normal distribution
float D_GGX(float NoH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = (NoH * NoH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Schlick-GGX geometry term (UE4 remap for direct lighting)
float G_SchlickGGX(float NoX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NoX / (NoX * (1.0 - k) + k);
}

float G_Smith(float NoV, float NoL, float roughness)
{
    return G_SchlickGGX(NoV, roughness) * G_SchlickGGX(NoL, roughness);
}

float3 F_Schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

[shader("fragment")]
float4 fragmentMain(Varyings i) : SV_Target
{
    // Debug view: flat cluster color (cluster-color on, lit-cluster-color off).
    // Bypasses lighting / tonemap / gamma so meshlet boundaries stay crisp.
    if (sceneData.debugParams.x > 0.5 && sceneData.debugParams.y < 0.5)
    {
        return float4(i.color, 1.0);
    }

    float3 albedo    = i.color * colorTex.Sample(i.uv).rgb;
    float3 mr        = metalRoughTex.Sample(i.uv).rgb;
    float  metallic  = mr.b * materialData.metalRoughFactors.x;
    float  roughness = clamp(mr.g * materialData.metalRoughFactors.y, 0.04, 1.0);

    float3 N = normalize(i.normal);
    float3 V = normalize(sceneData.cameraPos.xyz - i.worldPos);
    float3 L = normalize(sceneData.sunlightDirection.xyz);
    float3 H = normalize(V + L);

    float NoV = max(dot(N, V), 0.0);
    float NoL = max(dot(N, L), 0.0);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    // Dielectrics get F0 = 0.04, metals use albedo as F0.
    float3 F0 = lerp(float3(0.04), albedo, metallic);

    float  D = D_GGX(NoH, roughness);
    float  G = G_Smith(NoV, NoL, roughness);
    float3 F = F_Schlick(VoH, F0);

    float3 specular = (D * G) * F / max(4.0 * NoV * NoL, 1e-4);

    // Energy split: metals have no diffuse.
    float3 kd      = (float3(1.0) - F) * (1.0 - metallic);
    float3 diffuse = kd * albedo / PI;

    float3 sun    = sceneData.sunlightColor.rgb * sceneData.sunlightColor.w;
    float  shadow = computeShadow(i.worldPos, L, N, i.shadowCoord);
    float3 Lo     = (diffuse + specular) * sun * NoL * shadow;

    // Hemispheric ambient: blend sky/ground by N.y.
    float  upMix   = N.y * 0.5 + 0.5;
    float3 hemi    = lerp(sceneData.groundColor.rgb, sceneData.ambientColor.rgb, upMix);
    float3 ambient = hemi * albedo * (1.0 - metallic);

    // Cheap fill light from opposite side of the sun (horizontal mirror).
    float3 fillDir = normalize(float3(-L.x, max(L.y * 0.3, 0.0), -L.z));
    float  fillNoL = max(dot(N, fillDir), 0.0);
    float3 fill    = albedo * fillNoL * 0.35 * (1.0 - metallic);

    float3 color = ambient + fill + Lo;

    // Reinhard tonemap then gamma encode.
    color = color / (color + float3(1.0));
    color = pow(color, float3(1.0 / 2.2));

    return float4(color, 1.0);
}
