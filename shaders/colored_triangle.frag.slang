[shader("fragment")]
float4 main(float3 inColor : COLOR) : SV_Target
{
    return float4(inColor, 1.0);
}
