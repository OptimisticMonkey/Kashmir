// Hard-coded fullscreen triangle. No descriptors, no buffers — used for
// pipeline smoke tests.

struct VSOut
{
    float4 sv_position : SV_Position;
    float3 color       : COLOR;
};

[shader("vertex")]
VSOut main(uint vid : SV_VertexID)
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

    VSOut o;
    o.sv_position = float4(positions[vid], 1.0);
    o.color       = colors[vid];
    return o;
}
