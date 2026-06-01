// Hard-coded fullscreen triangle. No descriptors, no buffers — used for pipeline
// smoke tests. Combines:
//   vertexMain   -> colored_triangle.vert.spv
//   fragmentMain -> colored_triangle.frag.spv

// Shared interpolants — written by vertexMain, read by fragmentMain.
struct Varyings
{
    float4 sv_position : SV_Position;
    float3 color       : COLOR;
};

[shader("vertex")]
Varyings vertexMain(uint vid : SV_VertexID)
{
    static const float3 positions[3] = {
        float3( 1.0,  1.0, 0.0),
        float3(-1.0,  1.0, 0.0),
        float3( 0.0, -1.0, 0.0),
    };
    static const float3 colors[3] = {
        float3(1.0, 0.0, 0.0),
        float3(0.0, 1.0, 0.0),
        float3(0.0, 0.0, 1.0),
    };

    Varyings o;
    o.sv_position = float4(positions[vid], 1.0);
    o.color       = colors[vid];
    return o;
}

[shader("fragment")]
float4 fragmentMain(Varyings i) : SV_Target
{
    return float4(i.color, 1.0);
}
