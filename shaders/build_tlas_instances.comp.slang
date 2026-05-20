// Writes VkAccelerationStructureInstanceKHR records from a buffer of animated
// InstanceTransform (mat4) entries. Dispatched once per unique mesh, each
// dispatch fills `count` records starting at `outOffset` in the shared instance
// array. All records produced by a single dispatch share the same BLAS, mask,
// and flags — only the transform and customIndex vary per instance.
//
// Layout of AsInstance below matches VkAccelerationStructureInstanceKHR exactly
// (64 bytes total). The transform is a 3x4 row-major affine, which is what the
// AS build expects. shaderInt64 is avoided by splitting the BLAS device
// address into two uint32 halves on the C++ side.

import common;

struct AsInstance
{
    float4 r0;                   // 16 bytes — row 0 of the 3x4 transform
    float4 r1;                   // 16 bytes — row 1
    float4 r2;                   // 16 bytes — row 2
    uint   customIndexAndMask;   //  4 bytes — low 24 = instanceCustomIndex, high 8 = mask
    uint   sbtOffsetAndFlags;    //  4 bytes — low 24 = sbtRecordOffset (0), high 8 = flags
    uint2  blasReference;        //  8 bytes — .x = low32, .y = high32 of VkDeviceAddress
};

struct BuildTlasPC
{
    InstanceTransform* srcTransforms; // device pointer to InstanceTransform[count]
    AsInstance*        outInstances;  // device pointer to AsInstance[outOffset+count]
    uint               blasLow;       // low  32 bits of BLAS device address
    uint               blasHigh;      // high 32 bits of BLAS device address
    uint               count;
    uint               outOffset;
    uint               instanceCustomIndexBase;
    uint               flagsAndMask;  // low 8 = flags, next 8 = mask (top bits unused)
};
[[vk::push_constant]] BuildTlasPC pc;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint i = dtid.x;
    if (i >= pc.count) return;

    // Source is glm column-major in memory; Slang reads it back transposed,
    // so m[row][col] indexing is correct (matches update_transform.comp.slang).
    float4x4 m = pc.srcTransforms[i].transform;

    AsInstance dst;
    dst.r0 = float4(m[0][0], m[0][1], m[0][2], m[0][3]);
    dst.r1 = float4(m[1][0], m[1][1], m[1][2], m[1][3]);
    dst.r2 = float4(m[2][0], m[2][1], m[2][2], m[2][3]);

    uint flags = pc.flagsAndMask         & 0xFFu;
    uint mask  = (pc.flagsAndMask >> 8u) & 0xFFu;

    uint customIndex = pc.instanceCustomIndexBase + i;
    dst.customIndexAndMask = (customIndex & 0x00FFFFFFu) | (mask  << 24u);
    dst.sbtOffsetAndFlags  = (flags << 24u); // sbt record offset 0
    dst.blasReference      = uint2(pc.blasLow, pc.blasHigh);

    pc.outInstances[pc.outOffset + i] = dst;
}
