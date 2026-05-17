// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>


#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)


struct DrawContext;

// base class for a renderable dynamic object
class IRenderable {

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx, int InstanceCount) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : public IRenderable {

    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refreshTransform(worldTransform);
        }
    }

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx, int InstanceCount)
    {
        // draw children
        for (auto& c : children) {
            c->Draw(topMatrix, ctx, InstanceCount);
        }
    }
};

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct Vertex {

    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct InstanceTransform {
	glm::mat4 transform;
};

// One meshlet as consumed by the mesh shader. Layout matches the Slang
// `Meshlet` struct in shaders/mesh.mesh.slang (4 x uint32 = 16 bytes).
// Triangle offsets index a uint32 buffer where each entry packs one
// triangle's three byte indices into the low 24 bits.
struct GpuMeshlet {
    uint32_t vertexOffset;
    uint32_t triangleOffset;
    uint32_t vertexCount;
    uint32_t triangleCount;
};

// Per-meshlet bounding sphere + normal cone, produced by
// meshopt_computeMeshletBounds and consumed by the task shader. 48 bytes,
// vec4-aligned. Slang counterpart: `MeshletBounds` in shaders/mesh.task.slang.
struct GpuMeshletBounds {
    glm::vec4 centerRadius;   // xyz = bounding-sphere center, w = radius
    glm::vec4 coneApex;       // xyz = cone apex, w = padding
    glm::vec4 coneAxisCutoff; // xyz = cone axis, w = cos(angle/2)
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {

    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
    AllocatedBuffer instanceTransformBuffer;
    VkDeviceAddress instanceTransformBufferAddress;

    // Mesh-shader path. Populated by VulkanEngine::InitClusters; zero if
    // meshlets have not been built for this mesh.
    AllocatedBuffer meshletBuffer;
    AllocatedBuffer meshletVerticesBuffer;
    AllocatedBuffer meshletTrianglesBuffer;
    AllocatedBuffer meshletBoundsBuffer;
    VkDeviceAddress meshletBufferAddress{ 0 };
    VkDeviceAddress meshletVerticesAddress{ 0 };
    VkDeviceAddress meshletTrianglesAddress{ 0 };
    VkDeviceAddress meshletBoundsAddress{ 0 };
    uint32_t        meshletCount{ 0 };
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress instanceTransformBuffer;
};

// Push constants for the mesh-shader rendering path.
struct GPUMeshShaderPushConstants {
    glm::mat4       worldMatrix;             // 64
    VkDeviceAddress vertexBuffer;            //  8
    VkDeviceAddress instanceTransformBuffer; //  8
    VkDeviceAddress meshletBuffer;           //  8
    VkDeviceAddress meshletVertices;         //  8
    VkDeviceAddress meshletTriangles;        //  8
    VkDeviceAddress meshletBounds;           //  8
    uint32_t        meshletCount;            //  4
    uint32_t        instanceCount;           //  4
    uint32_t        debugFlags;              //  4  bit0 = cluster color, bit1 = lit cluster color
};
static_assert(sizeof(GPUMeshShaderPushConstants) <= 128, "Push constant size exceeds typical maxPushConstantsSize");

enum class MaterialPass :uint8_t {
    MainColor,
    Transparent,
    Other
};
struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};
struct DrawContext;

