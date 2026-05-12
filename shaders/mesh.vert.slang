import common;

struct PushConstants
{
    float4x4           renderMatrix;
    Vertex*            vertexBuffer;
    InstanceTransform* instanceTransformBuffer;
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
VSOut main(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    Vertex v = pc.vertexBuffer[vid];
    float4x4 instanceXform = pc.instanceTransformBuffer[iid].transform;

    float4 modelPos = float4(v.position, 1.0);
    float4 worldPos = mul(instanceXform, modelPos);

    VSOut o;
    o.sv_position = mul(sceneData.viewproj, worldPos);
    o.normal      = normalize(mul((float3x3)instanceXform, v.normal));
    o.color       = v.color.xyz * materialData.colorFactors.xyz;
    o.uv          = float2(v.uv_x, v.uv_y);
    o.worldPos    = worldPos.xyz;
    o.shadowCoord = mul(sceneData.lightViewProj, worldPos);
    return o;
}
