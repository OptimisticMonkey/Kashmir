// Editor grid overlay. Combines:
//   vertexMain   -> grid.vert.spv
//   fragmentMain -> grid.frag.spv

import common;

// Shared interpolants — written by vertexMain, read by fragmentMain.
struct Varyings
{
    float4 sv_position : SV_Position;
    float3 worldPos    : TEXCOORD0;
};

// ===========================================================================
// Vertex stage  ->  grid.vert.spv
// ===========================================================================
// Renders a single procedurally-uploaded quad (single instance, no per-instance
// transform buffer) positioned/scaled entirely via renderMatrix. Only the scene
// UBO (set 0) is consumed; the fragment stage derives the grid from world-space
// XZ, so no material data is needed here. The push-constant range still allocates
// room for the third pointer (shared layout) — we just don't read it.
struct PushConstants
{
    float4x4 renderMatrix;
    Vertex*  vertexBuffer;
};
[[vk::push_constant]] PushConstants pc;

[shader("vertex")]
Varyings vertexMain(uint vid : SV_VertexID)
{
    Vertex v = pc.vertexBuffer[vid];
    float4 worldPos = mul(pc.renderMatrix, float4(v.position, 1.0));

    Varyings o;
    o.sv_position = mul(sceneData.viewproj, worldPos);
    o.worldPos    = worldPos.xyz;
    return o;
}

// ===========================================================================
// Fragment stage  ->  grid.frag.spv
// ===========================================================================
// "Pristine grid" — from Ben Golus, "The Best Darn Grid Shader (Yet)".
// Uses screen-space derivatives to draw anti-aliased grid lines that stay crisp
// at any distance/angle and fade gracefully toward a solid tint once cells
// shrink below a pixel (no shimmering/moire).
//   uv        : grid-space coordinates (an integer step == one cell)
//   lineWidth : per-axis line thickness as a fraction of a cell, in [0,1]
//   returns   : line coverage in [0,1]
float pristineGrid(float2 uv, float2 lineWidth)
{
    float4 uvDDXY     = float4(ddx(uv), ddy(uv));
    float2 uvDeriv    = float2(length(uvDDXY.xz), length(uvDDXY.yw));
    bool2  invertLine = lineWidth > 0.5;
    float2 targetWidth = select(invertLine, 1.0 - lineWidth, lineWidth);
    float2 drawWidth  = clamp(targetWidth, uvDeriv, float2(0.5, 0.5));
    float2 lineAA     = uvDeriv * 1.5;
    float2 gridUV     = abs(frac(uv) * 2.0 - 1.0);
    gridUV = select(invertLine, gridUV, 1.0 - gridUV);
    float2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= saturate(targetWidth / drawWidth);
    grid2 = lerp(grid2, targetWidth, saturate(uvDeriv * 2.0 - 1.0));
    grid2 = select(invertLine, 1.0 - grid2, grid2);
    return lerp(grid2.x, 1.0, grid2.y);
}

[shader("fragment")]
float4 fragmentMain(Varyings i) : SV_Target
{
    // Grid is laid out on the world XZ plane. Spacing/width/color are fixed
    // constants — tweak here to restyle the editor grid.
    static const float  minorSpacing = 1.0;    // world units between thin lines
    static const float  majorSpacing = 10.0;   // world units between bright lines
    static const float2 minorWidth   = float2(0.02, 0.02); // ~0.02 world units thick
    static const float2 majorWidth   = float2(0.005, 0.005); // ~0.05 world units thick

    static const float3 minorColor  = float3(0.35, 0.35, 0.38);
    static const float3 majorColor  = float3(0.85, 0.85, 0.90);
    static const float  gridOpacity = 0.9;

    float minor = pristineGrid(i.worldPos.xz / minorSpacing, minorWidth);
    float major = pristineGrid(i.worldPos.xz / majorSpacing, majorWidth);

    // Major lines take over their color where they coincide with minor lines.
    float3 color = lerp(minorColor, majorColor, major);
    float  alpha = max(minor, major) * gridOpacity;

    return float4(color, alpha);
}
