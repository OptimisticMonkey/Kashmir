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
