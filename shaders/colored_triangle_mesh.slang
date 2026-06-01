// Debug pipeline that displays a textured mesh without lighting. Has its own
// descriptor layout and does NOT use common.slang. Combines:
//   vertexMain   -> colored_triangle_mesh.vert.spv
//   fragmentMain -> tex_image.frag.spv

struct DebugVertex
{
    float3 position;
    float  uv_x;
    float3 normal;
    float  uv_y;
    float4 color;
};

// Shared interpolants — written by vertexMain, read by fragmentMain.
struct Varyings
{
    float4 sv_position : SV_Position;
    float3 color       : COLOR;
    float2 uv          : TEXCOORD0;
};

// ===========================================================================
// Vertex stage  ->  colored_triangle_mesh.vert.spv
// ===========================================================================
struct PushConstants
{
    float4x4     renderMatrix;
    DebugVertex* vertexBuffer;
};
[[vk::push_constant]] PushConstants pc;

[shader("vertex")]
Varyings vertexMain(uint vid : SV_VertexID)
{
    DebugVertex v = pc.vertexBuffer[vid];

    Varyings o;
    o.sv_position = mul(pc.renderMatrix, float4(v.position, 1.0));
    o.color       = v.color.xyz;
    o.uv          = float2(v.uv_x, v.uv_y);
    return o;
}

// ===========================================================================
// Fragment stage  ->  tex_image.frag.spv
// Owns set 0 binding 0 as a single texture (NOT the SceneData UBO).
// ===========================================================================
[[vk::binding(0, 0)]] Sampler2D displayTexture;

[shader("fragment")]
float4 fragmentMain(Varyings i) : SV_Target
{
    return displayTexture.Sample(i.uv);
}
