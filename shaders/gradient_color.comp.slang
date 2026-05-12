[[vk::binding(0, 0)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> image;

struct GradientPC
{
    float4 data1;
    float4 data2;
    float4 data3;
    float4 data4;
};
[[vk::push_constant]] GradientPC pc;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    int2 texelCoord = int2(dtid.xy);
    uint2 size;
    image.GetDimensions(size.x, size.y);

    float4 topColor    = pc.data1;
    float4 bottomColor = pc.data2;

    if (texelCoord.x < (int)size.x && texelCoord.y < (int)size.y)
    {
        float blend = float(texelCoord.y) / float(size.y);
        image[texelCoord] = lerp(topColor, bottomColor, blend);
    }
}
