// gradient_color_hlsl.comp

// Storage image: set = 0, binding = 0
[[vk::binding(0, 0)]]
RWTexture2D<float4> image;

// Push constants
struct PushConstants {
    float4 data1;
    float4 data2;
    float4 data3;
    float4 data4;
};
[[vk::push_constant]]
ConstantBuffer<PushConstants> PC;


float3 palette(float t)
{
  float3 a = float3(0.5, 0.5, 0.5);
  float3 b = float3(0.5, 0.5, 0.5);
  float3 c = float3(1.0, 1.0, 0.5);
  float3 d = float3(0.0, 0.15, 0.2);

  return a + b * cos(6.28318f * (c * t + d));
}


[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint width, height;
    image.GetDimensions(width, height);
    uint2 iResolution = uint2(width,height);
    //printf("HELLO WORLD 12345 HLSL SHADER!!!  Thread = %u \n", DTid.x);
    float2 fragCoord = float2(DTid.x, DTid.y);  //Pixel Coords

    if(fragCoord.x >= iResolution.x || fragCoord.y >= iResolution.y)
        return;

    //if (DTid.x < width && DTid.y < height)
    //{
    //float iTime = 0.5f;
    float iTime = PC.data4.r;
    float2 uv = (fragCoord * 2.0f - iResolution) / iResolution.y; //0-1
    float2 uv0        = uv;
    float3 finalColor = float3(0.0, 0.0, 0.0);

    float iter = 1.0;
    float zoom = 1.0f;

    for(float i = 0.0; i < iter; i++)
    {
        uv = frac(uv * zoom) - 0.5f;

        float d = length(uv) * exp(-length(uv0));

        float3 col = palette(length(uv0) + i + iTime * .6);

        d = sin(d * 8 + iTime) / 2;
        d = abs(d);
        d = pow(0.01 / d, 1.2);

        finalColor += col * d;
    }

    //finalColor = float3(0, 1, 0);
    image[int2(fragCoord)] = float4(finalColor, 1.0F);
    //image[int2(DTid.xy)] = float4(finalColor, 1.0F);
    //}
}