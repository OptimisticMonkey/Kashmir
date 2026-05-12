import common;

// Ground draws don't use the per-instance transform buffer — they're single-
// instance with the world transform passed entirely in renderMatrix. The
// pipeline layout's push-constant range still allocates space for the third
// pointer; we just don't read it.
struct PushConstants
{
    float4x4 renderMatrix;
    Vertex*  vertexBuffer;
};
[[vk::push_constant]] PushConstants pc;

struct VSOut
{
    float4 sv_position : SV_Position;
    float3 normal      : NORMAL;
    float3 color       : COLOR;
    float2 uv          : TEXCOORD0;
    float3 worldPos    : TEXCOORD1;
    float4 shadowCoord : TEXCOORD2;
};

[shader("vertex")]
VSOut main(uint vid : SV_VertexID)
{
    Vertex v = pc.vertexBuffer[vid];
    float4 worldPos = mul(pc.renderMatrix, float4(v.position, 1.0));

    VSOut o;
    o.sv_position = mul(sceneData.viewproj, worldPos);
    o.normal      = normalize(mul((float3x3)pc.renderMatrix, v.normal));
    o.color       = v.color.xyz * materialData.colorFactors.xyz;
    o.uv          = float2(v.uv_x, v.uv_y);
    o.worldPos    = worldPos.xyz;
    o.shadowCoord = mul(sceneData.lightViewProj, worldPos);
    return o;
}
