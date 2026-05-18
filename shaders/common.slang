// Shared declarations for the engine's Slang shaders.
// Imported via `import common;` (slangc is invoked with `-I shaders/`).
//
// Layout note: SceneData/GLTFMaterialData come in via Vulkan UBOs that the
// C++ side fills with std140-equivalent layouts (GPUSceneData / GLTFMaterialData
// in vk_engine.h). Sticking with float4-aligned fields keeps Slang's default
// constant-buffer layout in agreement with what the engine writes.
//
// `Vertex` matches the vk-guide std430 layout: { vec3 pos, float u, vec3 n, float v, vec4 color }
// = 48 bytes. Slang's default packing for buffer-pointer dereferences matches std430.

public struct SceneData
{
    public float4x4 view;
    public float4x4 proj;
    public float4x4 viewproj;
    public float4x4 lightViewProj;
    public float4   ambientColor;
    public float4   sunlightDirection; // w = sun power
    public float4   sunlightColor;
    public float4   cameraPos;
    public float4   groundColor;
    public float4   shadowParams;      // .x = shadowMode (0 = rasterized map, 1 = raytraced TLAS)
    public float4   debugParams;       // .x = cluster color, .y = lit cluster color (mesh-shader debug)
};

public struct GLTFMaterialData
{
    public float4 colorFactors;
    public float4 metalRoughFactors;
};

public struct Vertex
{
    public float3 position;
    public float  uv_x;
    public float3 normal;
    public float  uv_y;
    public float4 color;
};

public struct InstanceTransform
{
    public float4x4 transform;
};

// Set 0 — per-frame scene globals.
[[vk::binding(0, 0)]] public ConstantBuffer<SceneData> sceneData;
[[vk::binding(1, 0)]] public Sampler2D                 shadowMap;
// Top-level acceleration structure for ray-queried shadows. When raytraced
// shadows are disabled (sceneData.shadowParams.x = 0) this binding points at
// an empty placeholder TLAS and is never consulted; the rasterized path is
// used instead.
[[vk::binding(2, 0)]] public RaytracingAccelerationStructure sceneTLAS;

// Set 1 — per-material.
[[vk::binding(0, 1)]] public ConstantBuffer<GLTFMaterialData> materialData;
[[vk::binding(1, 1)]] public Sampler2D                        colorTex;
[[vk::binding(2, 1)]] public Sampler2D                        metalRoughTex;

// Reverse-Z aware shadow lookup. Returns 1.0 = lit, 0.0 = shadowed.
// `lsPos` is the fragment's position in light clip space (sceneData.lightViewProj * worldPos).
public float sampleShadow(float4 lsPos)
{
    float3 p = lsPos.xyz / lsPos.w;
    // Light-space NDC -> [0,1] for X/Y. Z stays in [0,1] for Vulkan.
    p.xy = p.xy * 0.5 + 0.5;
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0)
        return 1.0;
    if (p.z < 0.0 || p.z > 1.0)
        return 1.0;
    float occluder = shadowMap.Sample(p.xy).r;
    // Reverse-Z: closer to light = larger depth value. Fragment is occluded
    // when it is farther from the light than the recorded occluder.
    const float bias = 0.0005;
    return (p.z + bias < occluder) ? 0.0 : 1.0;
}

// Hardware-raytraced shadow query. Casts an opaque-only visibility ray from
// `worldPos` (nudged off the surface along `N` to avoid self-intersection)
// toward `sunDir`. Returns 1.0 = lit, 0.0 = occluded — matching sampleShadow.
//
// AcceptFirstHit + EndSearch makes this a pure visibility test; the ray
// terminates the moment any triangle along the path is hit. All BLAS
// geometries are tagged OPAQUE so no any-hit shader is invoked.
public float traceShadowRay(float3 worldPos, float3 sunDir, float3 N)
{
    RayDesc ray;
    ray.Origin    = worldPos + N * 1e-3;
    ray.Direction = sunDir;
    ray.TMin      = 0.0;
    ray.TMax      = 1e4;

    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
    q.TraceRayInline(sceneTLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFFu, ray);
    q.Proceed();
    return (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.0 : 1.0;
}

// Convenience wrapper — picks rasterized vs ray-queried shadow based on
// sceneData.shadowParams.x. `shadowCoord` is the light-space position used
// by the rasterized path; the RT path ignores it.
public float computeShadow(float3 worldPos, float3 sunDir, float3 N, float4 shadowCoord)
{
    if (sceneData.shadowParams.x > 0.5)
    {
        return traceShadowRay(worldPos, sunDir, N);
    }
    return sampleShadow(shadowCoord);
}
