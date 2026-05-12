// Debug fragment shader paired with colored_triangle_mesh.vert. Owns set 0
// binding 0 as a single texture (NOT the SceneData UBO from common.slang).

[[vk::binding(0, 0)]] Sampler2D displayTexture;

struct FSIn
{
    float3 color : COLOR;
    float2 uv    : TEXCOORD0;
};

[shader("fragment")]
float4 main(FSIn i) : SV_Target
{
    return displayTexture.Sample(i.uv);
}
