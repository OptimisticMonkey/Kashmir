//// newgradient.hlsl

//// Push constants
//struct Push
//{
//    int2 imageSize; // match what you set in vkCmdPushConstants
//};
//[[vk::push_constant]]
//ConstantBuffer<Push> pc;

//// Storage image (set=0, binding=0)  -> register(u0, space0)
//RWTexture2D<float4> img : register(u0, space0);

//// Workgroup size
//[numthreads(16, 16, 1)]
//void main(uint3 DTid : SV_DispatchThreadID)
//{
//    int2 pix = int2(DTid.xy);
//    if (pix.x >= pc.imageSize.x || pix.y >= pc.imageSize.y)
//        return;

//    float2 uv = float2(pix) / float2(pc.imageSize);
//    float3 color = float3(uv, 0.0f);
//    img[pix] = float4(color, 1.0f);
//}

// gradient.comp.hlsl
// set=0, binding=0  ->  register(u0, space0)  (adjust if your descriptor set/binding differ)
RWTexture2D<float4> imageTex : register(u0, space0);

// GLSL: layout(local_size_x=16, local_size_y=16)
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID, // GLSL gl_GlobalInvocationID
          uint3 GTid : SV_GroupThreadID)      // GLSL gl_LocalInvocationID
{
    uint w, h;
    imageTex.GetDimensions(w, h);

    uint2 texelCoord = DTid.xy;
    printf("HELLO HLSL SHADER!!!  Thread = %d \n", GTid.x);
    if (texelCoord.x < w && texelCoord.y < h)
    {
        float4 color = float4(0.0, 0.0, 0.0, 1.0);

        if (GTid.x != 0 && GTid.y != 0)
        {
            color.x = float(texelCoord.x) / float(w);
            color.y = float(texelCoord.y) / float(h);
        }

        //float4 color2 = float4(1.0, 1.0, 1.0, 1.0);
        imageTex[int2(texelCoord)] = color;
    }
}