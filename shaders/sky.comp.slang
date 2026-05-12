// Starfield generator. Background-compute background.
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

[[vk::binding(0, 0)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> image;

float Noise2d(float2 x)
{
    float xhash = cos(x.x * 37.0);
    float yhash = cos(x.y * 57.0);
    return frac(415.92653 * (xhash + yhash));
}

float NoisyStarField(float2 samplePos, float threshold)
{
    float StarVal = Noise2d(samplePos);
    if (StarVal >= threshold)
        StarVal = pow((StarVal - threshold) / (1.0 - threshold), 6.0);
    else
        StarVal = 0.0;
    return StarVal;
}

float StableStarField(float2 samplePos, float threshold)
{
    // Linear interpolation between four samples.
    float fractX = frac(samplePos.x);
    float fractY = frac(samplePos.y);
    float2 floorSample = floor(samplePos);
    float v1 = NoisyStarField(floorSample,                     threshold);
    float v2 = NoisyStarField(floorSample + float2(0.0, 1.0),  threshold);
    float v3 = NoisyStarField(floorSample + float2(1.0, 0.0),  threshold);
    float v4 = NoisyStarField(floorSample + float2(1.0, 1.0),  threshold);

    return  v1 * (1.0 - fractX) * (1.0 - fractY)
          + v2 * (1.0 - fractX) *        fractY
          + v3 *        fractX  * (1.0 - fractY)
          + v4 *        fractX  *        fractY;
}

float4 mainImage(float2 fragCoord, float2 iResolution)
{
    float3 vColor = float3(0.1, 0.2, 0.4) * fragCoord.y / iResolution.y;

    // Higher threshold (closer to 1.0) yields a sparser starfield.
    const float StarFieldThreshold = 0.97;

    // Stars with a slow crawl.
    const float xRate = 0.2;
    const float yRate = -0.06;
    float2 vSamplePos = fragCoord + float2(xRate, yRate);
    float StarVal = StableStarField(vSamplePos, StarFieldThreshold);
    vColor += float3(StarVal);

    return float4(vColor, 1.0);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    int2 texelCoord = int2(dtid.xy);
    uint2 size;
    image.GetDimensions(size.x, size.y);
    if (texelCoord.x < (int)size.x && texelCoord.y < (int)size.y)
    {
        float4 color = mainImage(float2(texelCoord), float2(size));
        image[texelCoord] = color;
    }
}
