// Shadow caster vertex shader — re-renders OpaqueSurfaces from the sun's
// perspective using the same per-draw buffer-device-address inputs as
// mesh.vert.

import common;

struct PushConstants
{
    float4x4           renderMatrix;
    Vertex*            vertexBuffer;
    InstanceTransform* instanceTransformBuffer;
};
[[vk::push_constant]] PushConstants pc;

[shader("vertex")]
float4 main(uint vid : SV_VertexID, uint iid : SV_InstanceID) : SV_Position
{
    Vertex v = pc.vertexBuffer[vid];
    float4x4 instanceXform = pc.instanceTransformBuffer[iid].transform;
    float4 worldPos = mul(instanceXform, float4(v.position, 1.0));
    return mul(sceneData.lightViewProj, worldPos);
}
