// Per-instance transform animation. Writes through a VkDeviceAddress to the
// same buffer that mesh.vert / shadow.vert read.

import common;

struct UpdateTransformPC
{
    InstanceTransform* instanceTransformBuffer; // VK_KHR_buffer_device_address
    float              time;
    uint               count;
    float              padX;
    float              padY;
};
[[vk::push_constant]] UpdateTransformPC pc;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint i = dtid.x;
    if (i >= pc.count) return;

    // Preserve the existing translation, rebuild rotation each frame.
    // Slang/HLSL source matrices are row-major: `m[i][j]` is row i, col j.
    // The CPU uploads glm column-major data; Slang reads it back transposed
    // so source row indexing works as expected.
    float4x4 cur = pc.instanceTransformBuffer[i].transform;
    float3 pos = float3(cur[0][3], cur[1][3], cur[2][3]);

    // Instance 0 is driven by the gamepad left stick.
    if (i == 0u)
    {
        pos.x += pc.padX * 0.001;
        pos.y += pc.padY * 0.001;
    }

    float angle = pc.time + float(i) * 0.3;
    float c = cos(angle);
    float s = sin(angle);

    // Y-rotation + translation, written in Slang row-major source order.
    // (Storage to the buffer is automatically column-major, matching glm.)
    float4x4 R = float4x4(
        c,    0.0,   s,    pos.x,
        0.0,  1.0,   0.0,  pos.y,
       -s,    0.0,   c,    pos.z,
        0.0,  0.0,   0.0,  1.0
    );

    pc.instanceTransformBuffer[i].transform = R;
}
