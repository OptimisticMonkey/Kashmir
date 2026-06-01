import common;

// Editor-grid vertex shader. Renders a single procedurally-uploaded quad
// (single instance, no per-instance transform buffer) positioned/scaled
// entirely via renderMatrix. Only the scene UBO (set 0) is consumed; the
// fragment stage derives the grid from world-space XZ, so no material data is
// needed here. The push-constant range still allocates room for the third
// pointer (shared layout) — we just don't read it.
struct PushConstants
{
    float4x4 renderMatrix;
    Vertex*  vertexBuffer;
};
[[vk::push_constant]] PushConstants pc;

struct VSOut
{
    float4 sv_position : SV_Position;
    float3 worldPos    : TEXCOORD0;
};

[shader("vertex")]
VSOut main(uint vid : SV_VertexID)
{
    Vertex v = pc.vertexBuffer[vid];
    float4 worldPos = mul(pc.renderMatrix, float4(v.position, 1.0));

    VSOut o;
    o.sv_position = mul(sceneData.viewproj, worldPos);
    o.worldPos    = worldPos.xyz;
    return o;
}
