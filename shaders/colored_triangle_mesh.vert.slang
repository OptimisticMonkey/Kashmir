// Debug pipeline that displays a textured mesh without lighting. Has its own
// descriptor layout (paired with tex_image.frag) and does not use common.slang.

struct DebugVertex
{
    float3 position;
    float  uv_x;
    float3 normal;
    float  uv_y;
    float4 color;
};

struct PushConstants
{
    float4x4     renderMatrix;
    DebugVertex* vertexBuffer;
};
[[vk::push_constant]] PushConstants pc;

struct VSOut
{
    float4 sv_position : SV_Position;
    float3 color       : COLOR;
    float2 uv          : TEXCOORD0;
};

[shader("vertex")]
VSOut main(uint vid : SV_VertexID)
{
    DebugVertex v = pc.vertexBuffer[vid];

    VSOut o;
    o.sv_position = mul(pc.renderMatrix, float4(v.position, 1.0));
    o.color       = v.color.xyz;
    o.uv          = float2(v.uv_x, v.uv_y);
    return o;
}
