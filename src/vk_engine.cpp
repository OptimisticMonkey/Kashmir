//> includes
#define VMA_STATS_STRING_ENABLED 1
#define VMA_IMPLEMENTATION
#include "vk_engine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <glm/gtx/transform.hpp>
#include <vk_initializers.h>
#include <vk_images.h>
#include <vk_types.h>
#include <vk_pipelines.h>

// bootstrap library
#include "VkBootstrap.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include <chrono>
#include <cmath>
#include <random>
#include <unordered_set>
#include <thread>

#include <meshoptimizer.h>

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get()
{
    return *loadedEngine;
}

constexpr bool bUseValidationLayers = true;

void VulkanEngine::init()
{
    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

    int num_joysticks = 0;
    SDL_JoystickID* joystick_ids = SDL_GetJoysticks(&num_joysticks);
    if (joystick_ids)
    {
        for (int i = 0; i < num_joysticks; i++)
        {
            SDL_JoystickID jid = joystick_ids[i];
            if (SDL_IsGamepad(jid))
            {
                _controller = SDL_OpenGamepad(jid);
                if (_controller)
                {
                    fmt::print("Opened gamepad: {}\n", SDL_GetGamepadName(_controller));
                    break;
                }
            }
        }
        SDL_free(joystick_ids);
    }

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow("Vulkan Engine",
                               _windowExtent.width,
                               _windowExtent.height,
                               window_flags);

    init_vulkan();

    init_swapchain();

    init_commands();

    init_sync_structures();

    init_descriptors();

    init_pipelines();

    init_raytracing();

    init_imgui();

    init_default_data();

    mainCamera.velocity = glm::vec3(0.f);
    mainCamera.position = glm::vec3(0, 0, 5);
    // mainCamera.position = glm::vec3(30.f, -00.f, -085.f);

    mainCamera.pitch = 0;
    mainCamera.yaw = 0;

    // std::string structurePath = { "..\\..\\assets\\Simple.glb" };
    // std::string structurePath = { "..\\..\\assets\\Suzanne.glb" };
    // std::string structurePath = { "..\\..\\assets\\Torus.glb" };
    // std::string structurePath = { "..\\..\\assets\\structure.glb" };
    // std::string structurePath = { "..\\..\\assets\\3boxes.glb" };
    // std::string structurePath = { "..\\..\\assets\\DamagedHelmet.glb" };
    // std::string structurePath = { "..\\..\\assets\\BasicMesh.glb" };

    // auto structureFile = loadGltf(this, structurePath);

    // assert(structureFile.has_value());

    // loadedScenes["structure"] = *structureFile;

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::init_default_data()
{
    // std::array<Vertex, 4> rect_vertices;

    // rect_vertices[0].position = { 0.5,-0.5, 0 };
    // rect_vertices[1].position = { 0.5,0.5, 0 };
    // rect_vertices[2].position = { -0.5,-0.5, 0 };
    // rect_vertices[3].position = { -0.5,0.5, 0 };

    // rect_vertices[0].color = { 0,0, 0,1 };
    // rect_vertices[1].color = { 0.5,0.5,0.5 ,1 };
    // rect_vertices[2].color = { 1,0, 0,1 };
    // rect_vertices[3].color = { 0,1, 0,1 };

    // std::array<uint32_t, 6> rect_indices;

    // rect_indices[0] = 0;
    // rect_indices[1] = 1;
    // rect_indices[2] = 2;

    // rect_indices[3] = 2;
    // rect_indices[4] = 1;
    // rect_indices[5] = 3;

    // rectangle = uploadMesh(rect_indices, rect_vertices);

    ////delete the rectangle data on engine shutdown
    //_mainDeletionQueue.push_function([&]() {
    //    destroy_buffer(rectangle.indexBuffer);
    //    destroy_buffer(rectangle.vertexBuffer);
    //    destroy_buffer(rectangle.instanceTransformBuffer);
    //    });

    // testMeshes = loadGltfMeshes(this, "..\\..\\assets\\basicmesh.glb").value();
    testMeshes = loadGltfMeshes(this, "..\\..\\assets\\Suzanne.glb").value();

    // 3 default textures, white, grey, black. 1 pixel each
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage = create_image((void*)&white,
                               VkExtent3D{ 1, 1, 1 },
                               VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                               false,
                               "WhiteImage");

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    _greyImage = create_image(
        (void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false, "GreyImage");

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    _blackImage = create_image((void*)&black,
                               VkExtent3D{ 1, 1, 1 },
                               VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                               false,
                               "BlackImage");

    // checkerboard image
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16> pixels; // for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    _errorCheckerboardImage = create_image(pixels.data(),
                                           VkExtent3D{ 16, 16, 1 },
                                           VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_USAGE_SAMPLED_BIT,
                                           false,
                                           "ErrorCheckerboardImage");

    VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear);

    _mainDeletionQueue.push_function(
        [&]()
        {
            vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
            vkDestroySampler(_device, _defaultSamplerLinear, nullptr);

            destroy_image(_whiteImage);
            destroy_image(_greyImage);
            destroy_image(_blackImage);
            destroy_image(_errorCheckerboardImage);
        });

    GLTFMetallic_Roughness::MaterialResources materialResources;
    // default the material textures
    materialResources.colorImage = _whiteImage;
    materialResources.colorSampler = _defaultSamplerLinear;
    materialResources.metalRoughImage = _whiteImage;
    materialResources.metalRoughSampler = _defaultSamplerLinear;

    // set the uniform buffer for the material data
    AllocatedBuffer materialConstants = create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants),
                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                      VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                      "MaterialConstants");

    // write the buffer
    GLTFMetallic_Roughness::MaterialConstants* sceneUniformData =
        (GLTFMetallic_Roughness::MaterialConstants*)materialConstants.allocation->GetMappedData();
    sceneUniformData->colorFactors = glm::vec4{ 1, 1, 1, 1 };
    sceneUniformData->metal_rough_factors = glm::vec4{ 1, 0.5, 0, 0 };

    _mainDeletionQueue.push_function([=, this]() { destroy_buffer(materialConstants); });

    materialResources.dataBuffer = materialConstants.buffer;
    materialResources.dataBufferOffset = 0;

    defaultData = metalRoughMaterial.write_material(
        _device, MaterialPass::MainColor, materialResources, globalDescriptorAllocator);

    // Ground shares the default descriptor set but uses its own pipeline.
    _groundMaterial = defaultData;
    _groundMaterial.pipeline = &_groundPipeline;

    for (auto& m : testMeshes)
    {
        std::shared_ptr<MeshNode> newNode = std::make_shared<MeshNode>();
        newNode->mesh = m;

        newNode->localTransform = glm::mat4{ 1.f };
        newNode->worldTransform = glm::mat4{ 1.f };

        for (auto& s : newNode->mesh->surfaces)
        {
            s.material = std::make_shared<GLTFMaterial>(defaultData);
        }

        loadedNodes[m->name] = std::move(newNode);
    }

    // Ground mesh — single-instance static geometry, uses ground.vert (no instance transform buffer).
    auto groundMeshes = loadGltfMeshes(this, "..\\..\\assets\\Ground.glb");
    if (groundMeshes.has_value() && !groundMeshes->empty())
    {
        // Reuse testMeshes for buffer ownership so the existing cleanup loop
        // destroys every ground mesh's buffers (not just the one we draw).
        for (auto& m : *groundMeshes)
        {
            for (auto& s : m->surfaces)
            {
                s.material = std::make_shared<GLTFMaterial>(_groundMaterial);
            }
            testMeshes.push_back(m);
        }

        _groundNode = std::make_shared<MeshNode>();
        _groundNode->mesh = groundMeshes->front();
        _groundNode->localTransform = glm::mat4{ 1.f };
        _groundNode->worldTransform = glm::mat4{ 1.f };
    }
    else
    {
        fmt::println("Failed to load Ground.glb");
    }
}

void VulkanEngine::init_pipelines()
{

    init_background_pipelines();
    init_update_transform_pipeline();
    init_shadow_resources();
    init_shadow_pipeline();
    init_triangle_pipeline();
    init_mesh_pipeline();
    metalRoughMaterial.build_pipelines(this);
    init_ground_pipeline();
    init_tlas_instance_pipeline();
}

void VulkanEngine::init_ground_pipeline()
{
    VkShaderModule fragShader;
    if (!vkutil::load_shader_module("../../shaders/ground.frag.spv", _device, &fragShader))
    {
        fmt::println("Error loading ground.frag.spv for ground pipeline");
    }
    VkShaderModule vertShader;
    if (!vkutil::load_shader_module("../../shaders/ground.vert.spv", _device, &vertShader))
    {
        fmt::println("Error loading ground.vert.spv");
    }

    // Share layout with metalRoughMaterial — same descriptor sets, same push-constant range.
    _groundPipeline.layout = metalRoughMaterial.opaquePipeline.layout;

    PipelineBuilder pb;
    pb.set_shaders(vertShader, fragShader);
    pb.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pb.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pb.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pb.set_multisampling_none();
    pb.disable_blending();
    pb.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pb.set_color_attachment_format(_drawImage.imageFormat);
    pb.set_depth_format(_depthImage.imageFormat);
    pb._pipelineLayout = _groundPipeline.layout;

    _groundPipeline.pipeline = pb.build_pipeline(_device);

    vkDestroyShaderModule(_device, fragShader, nullptr);
    vkDestroyShaderModule(_device, vertShader, nullptr);

    _mainDeletionQueue.push_function(
        [=]()
        {
            vkDestroyPipeline(_device, _groundPipeline.pipeline, nullptr);
            // layout is owned by metalRoughMaterial, don't destroy here
        });
}

void VulkanEngine::init_shadow_resources()
{
    // Offscreen depth target sampled by lighting shaders. Sized independently of
    // the swapchain (so a window resize does not invalidate it).
    VkExtent3D extent{ _shadowExtent.width, _shadowExtent.height, 1 };
    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    _shadowImage = create_image(extent, VK_FORMAT_D32_SFLOAT, usage, false, "ShadowMap");

    // Sampler used by mesh.frag / ground.frag to read the shadow map.
    // clamp-to-border with white border = "lit" for fragments outside the light's frustum.
    VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    // Reverse-Z: large depth value = close to light = "fully lit".
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK(vkCreateSampler(_device, &samplerInfo, nullptr, &_shadowSampler));

    AllocatedImage shadowImage = _shadowImage;
    VkSampler shadowSampler = _shadowSampler;
    VkDevice device = _device;
    VmaAllocator allocator = _allocator;
    _mainDeletionQueue.push_function(
        [shadowImage, shadowSampler, device, allocator]()
        {
            vkDestroySampler(device, shadowSampler, nullptr);
            vkDestroyImageView(device, shadowImage.imageView, nullptr);
            vmaDestroyImage(allocator, shadowImage.image, shadowImage.allocation);
        });
}

void VulkanEngine::init_shadow_pipeline()
{
    VkShaderModule shadowVert;
    if (!vkutil::load_shader_module("../../shaders/shadow.vert.spv", _device, &shadowVert))
    {
        fmt::println("Error loading shadow.vert.spv for shadow pipeline");
    }
    VkShaderModule shadowFrag;
    if (!vkutil::load_shader_module("../../shaders/shadow.frag.spv", _device, &shadowFrag))
    {
        fmt::println("Error loading shadow.frag.spv for shadow pipeline");
    }

    // Shadow pass uses the scene UBO for `lightViewProj` (set 0) and the same
    // GPUDrawPushConstants layout as the camera pass (so we can iterate the
    // existing OpaqueSurfaces without rebuilding any data).
    VkPushConstantRange pushRange{};
    pushRange.offset = 0;
    pushRange.size = sizeof(GPUDrawPushConstants);
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout setLayouts[] = { _gpuSceneDataDescriptorLayout };
    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_shadowPipelineLayout));

    PipelineBuilder pb;
    pb.set_shaders(shadowVert, shadowFrag);
    pb.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pb.set_polygon_mode(VK_POLYGON_MODE_FILL);
    // Front-face culling reduces self-shadow acne on caster geometry.
    pb.set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE);
    pb.set_multisampling_none();
    pb.disable_blending();
    // Reverse-Z: clear to 0, keep larger values (closer to light).
    pb.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pb.set_depth_format(_shadowImage.imageFormat);
    // No color attachment — depth-only render. (PipelineBuilder handles
    // colorAttachmentCount == 0.)
    pb._pipelineLayout = _shadowPipelineLayout;

    _shadowPipeline = pb.build_pipeline(_device);

    vkDestroyShaderModule(_device, shadowVert, nullptr);

    // --- Mesh-shader variant of the shadow caster pipeline. Shares the
    // GPUMeshShaderPushConstants layout with the main mesh pipeline so we can
    // forward the same data from draw_shadow.
    VkShaderModule shadowMesh;
    if (!vkutil::load_shader_module("../../shaders/shadow.mesh.spv", _device, &shadowMesh))
    {
        fmt::println("Error loading shadow.mesh.spv for shadow pipeline");
    }

    VkPushConstantRange meshPushRange{};
    meshPushRange.offset = 0;
    meshPushRange.size = sizeof(GPUMeshShaderPushConstants);
    meshPushRange.stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT;

    VkPipelineLayoutCreateInfo meshLayoutInfo = vkinit::pipeline_layout_create_info();
    meshLayoutInfo.setLayoutCount = 1;
    meshLayoutInfo.pSetLayouts = setLayouts;
    meshLayoutInfo.pushConstantRangeCount = 1;
    meshLayoutInfo.pPushConstantRanges = &meshPushRange;
    VK_CHECK(vkCreatePipelineLayout(_device, &meshLayoutInfo, nullptr, &_shadowMeshPipelineLayout));

    PipelineBuilder mpb;
    mpb.set_mesh_shaders(VK_NULL_HANDLE, shadowMesh, shadowFrag);
    mpb.set_polygon_mode(VK_POLYGON_MODE_FILL);
    mpb.set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE);
    mpb.set_multisampling_none();
    mpb.disable_blending();
    mpb.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    mpb.set_depth_format(_shadowImage.imageFormat);
    mpb._pipelineLayout = _shadowMeshPipelineLayout;

    _shadowMeshPipeline = mpb.build_pipeline(_device);

    vkDestroyShaderModule(_device, shadowMesh, nullptr);
    vkDestroyShaderModule(_device, shadowFrag, nullptr);

    _mainDeletionQueue.push_function(
        [=]()
        {
            vkDestroyPipelineLayout(_device, _shadowPipelineLayout, nullptr);
            vkDestroyPipeline(_device, _shadowPipeline, nullptr);
            vkDestroyPipelineLayout(_device, _shadowMeshPipelineLayout, nullptr);
            vkDestroyPipeline(_device, _shadowMeshPipeline, nullptr);
        });
}

void VulkanEngine::init_update_transform_pipeline()
{
    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(UpdateTransformPushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;
    layoutInfo.setLayoutCount = 0;
    VK_CHECK(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_updateTransformPipelineLayout));

    VkShaderModule shader;
    if (!vkutil::load_shader_module("../../shaders/update_transform.comp.spv", _device, &shader))
    {
        fmt::print("Error when building the update_transform compute shader \n");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = shader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo pipeInfo{};
    pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeInfo.layout = _updateTransformPipelineLayout;
    pipeInfo.stage = stageinfo;
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &_updateTransformPipeline));

    vkDestroyShaderModule(_device, shader, nullptr);

    _mainDeletionQueue.push_function(
        [=]()
        {
            vkDestroyPipelineLayout(_device, _updateTransformPipelineLayout, nullptr);
            vkDestroyPipeline(_device, _updateTransformPipeline, nullptr);
        });
}

void VulkanEngine::init_triangle_pipeline()
{
    VkShaderModule triangleFragShader;
    if (!vkutil::load_shader_module("../../shaders/colored_triangle.frag.spv", _device, &triangleFragShader))
    {
        fmt::print("Error when building the triangle fragment shader module (colored_triangle.frag.spv) \n");
    }
    else
    {
        fmt::print("Triangle fragment shader succesfully loaded (colored_triangle.frag.spv)\n");
    }

    VkShaderModule triangleVertexShader;
    if (!vkutil::load_shader_module("../../shaders/colored_triangle.vert.spv", _device, &triangleVertexShader))
    {
        fmt::print("Error when building the triangle vertex shader module(colored_triangle.vert.spv)\n");
    }
    else
    {
        fmt::print("Triangle vertex shader succesfully loaded (colored_triangle.vert.spv)\n");
    }

    // build the pipeline layout that controls the inputs/outputs of the shader
    // we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));

    PipelineBuilder pipelineBuilder;

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
    // connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
    // it will draw triangles
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    // filled triangles
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    // no backface culling
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    // no multisampling
    pipelineBuilder.set_multisampling_none();
    // no blending
    pipelineBuilder.disable_blending();
    // no depth testing
    pipelineBuilder.disable_depthtest();

    // connect the image format we will draw into, from draw image
    pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(_depthImage.imageFormat);

    // finally build the pipeline
    _trianglePipeline = pipelineBuilder.build_pipeline(_device);

    // clean structures
    vkDestroyShaderModule(_device, triangleFragShader, nullptr);
    vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

    _mainDeletionQueue.push_function(
        [&]()
        {
            vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
            vkDestroyPipeline(_device, _trianglePipeline, nullptr);
        });
}

void VulkanEngine::init_background_pipelines()
{
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

    // VkShaderModule computeDrawShader;
    ////if (!vkutil::load_shader_module("../../shaders/gradient.comp.spv", _device, &computeDrawShader))
    // if (!vkutil::load_shader_module("../../shaders/newgradient.spv", _device, &computeDrawShader))
    //{
    //     fmt::print("Error when building the compute shader \n");
    // }
    VkShaderModule computeDrawShader;
    if (!vkutil::load_shader_module("../../shaders/gradient_color.comp.spv", _device, &computeDrawShader))
    // if (!vkutil::load_shader_module("../../shaders/sky.comp.spv", _device, &computeDrawShader))
    {
        fmt::print("Error when building the colored mesh shader \n");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeDrawShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    // VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr,
    // &_gradientPipeline));

    //----GRADIENT SHADER----
    VkShaderModule gradientShader;
    // if (!vkutil::load_shader_module("../../shaders/gradient_color.comp.spv", _device, &gradientShader))
    // if (!vkutil::load_shader_module("../../shaders/gradient_color_hlsl.spv", _device, &gradientShader))
    if (!vkutil::load_shader_module("../../shaders/shapes.spv", _device, &gradientShader))
    {
        fmt::print("Error when building the gradient compute shader \n");
    }
    ComputeEffect gradient;
    gradient.layout = _gradientPipelineLayout;
    gradient.name = "gradient";
    gradient.data = {};

    // default colors
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    // stageinfo.module = gradientShader;
    computePipelineCreateInfo.stage.module = gradientShader;
    VK_CHECK(
        vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));
    //----END GRADIENT SHADER----

    //----SKY SHADER----
    VkShaderModule skyShader;
    if (!vkutil::load_shader_module("../../shaders/sky.comp.spv", _device, &skyShader))
    {
        fmt::print("Error when building the sky compute shader \n");
    }
    // change the shader module only to create the sky shader
    computePipelineCreateInfo.stage.module = skyShader;
    ComputeEffect sky;
    sky.layout = _gradientPipelineLayout;
    sky.name = "sky";
    sky.data = {};
    // default sky parameters
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);
    // stageinfo.module = skyShader;
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));
    //----END SKY SHADER----

    //----------------GRID SHADER
    VkShaderModule gridShader;
    if (!vkutil::load_shader_module("../../shaders/grid.comp.spv", _device, &gridShader))
    {
        fmt::print("Error when building the grid compute shader \n");
    }

    ComputeEffect grid;
    grid.layout = _gradientPipelineLayout;
    grid.name = "grid";
    grid.data = {};
    grid.data.data1 = glm::vec4(1, 0, 0, 1);
    grid.data.data2 = glm::vec4(0, 0, 1, 1);

    // stageinfo.module = gridShader;
    computePipelineCreateInfo.stage.module = gridShader;
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &grid.pipeline));
    //----END GRID SHADER----

    //----TUNNEL SHADER----
    VkShaderModule TunnelShader;
    // if (!vkutil::load_shader_module("../../shaders/gradient_color.comp.spv", _device, &gradientShader))
    if (!vkutil::load_shader_module("../../shaders/tunnel.comp.spv", _device, &TunnelShader))
    {
        fmt::print("Error when building the tunnel compute shader \n");
    }

    ComputeEffect TunnelEffect;
    TunnelEffect.layout = _gradientPipelineLayout;
    TunnelEffect.name = "Tunnel";
    TunnelEffect.data = {};

    // default colors
    TunnelEffect.data.data1 = glm::vec4(1, 0, 0, 1);
    TunnelEffect.data.data2 = glm::vec4(0, 0, 1, 1);

    // stageinfo.module = TunnelShader;
    computePipelineCreateInfo.stage.module = TunnelShader;
    VK_CHECK(vkCreateComputePipelines(
        _device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &TunnelEffect.pipeline));
    //----END TUNNEL SHADER----

    // add the 2 background effects into the array
    backgroundEffects.push_back(sky);
    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(grid);
    backgroundEffects.push_back(TunnelEffect);

    // destroy structures properly
    vkDestroyShaderModule(_device, computeDrawShader, nullptr);
    vkDestroyShaderModule(_device, gradientShader, nullptr);
    vkDestroyShaderModule(_device, skyShader, nullptr);
    vkDestroyShaderModule(_device, gridShader, nullptr);
    vkDestroyShaderModule(_device, TunnelShader, nullptr);
    _mainDeletionQueue.push_function(
        [=]()
        {
            vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
            vkDestroyPipeline(_device, sky.pipeline, nullptr);
            vkDestroyPipeline(_device, gradient.pipeline, nullptr);
            vkDestroyPipeline(_device, grid.pipeline, nullptr);
            vkDestroyPipeline(_device, TunnelEffect.pipeline, nullptr);
            // vkDestroyPipeline(_device, _gradientPipeline, nullptr);
        });
}

void VulkanEngine::init_descriptors()
{
    // create a descriptor pool that will hold 10 sets with 1 image each
    // std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    //{
    //     { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    // };

    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    };
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes2 = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
    };

    // globalDescriptorAllocator.init_pool(_device, 10, sizes);
    globalDescriptorAllocator.init(_device, 10, sizes2);

    // make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        _singleImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    // allocate a descriptor set for our draw image
    _drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

    // replaced with new descriptor stuff
    /* VkDescriptorImageInfo imgInfo{};
     imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
     imgInfo.imageView = _drawImage.imageView;

     VkWriteDescriptorSet drawImageWrite = {};
     drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
     drawImageWrite.pNext = nullptr;

     drawImageWrite.dstBinding = 0;
     drawImageWrite.dstSet = _drawImageDescriptors;
     drawImageWrite.descriptorCount = 1;
     drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
     drawImageWrite.pImageInfo = &imgInfo;

     vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);*/

    DescriptorWriter writer;
    writer.write_image(
        0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(_device, _drawImageDescriptors);

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // shadow map
        builder.add_binding(2, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR); // raytraced-shadow TLAS
        // TASK_BIT_EXT is needed because mesh.task.slang reads sceneData.viewproj
        // for the per-meshlet frustum cull. Layout was missing this stage prior
        // to raytraced shadows; the new TLAS binding (only read in FRAGMENT) made
        // validation flag it during the wider descriptor sweep.
        _gpuSceneDataDescriptorLayout =
            builder.build(_device,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TASK_BIT_EXT |
                              VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    // make sure both the descriptor allocator and the new layout get cleaned up properly
    _mainDeletionQueue.push_function(
        [&]()
        {
            // globalDescriptorAllocator.destroy_pool(_device);
            globalDescriptorAllocator.destroy_pools(_device);

            vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
            vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout, nullptr);
            vkDestroyDescriptorSetLayout(_device, _singleImageDescriptorLayout, nullptr);
        });

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
            { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 2 },
        };

        _frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
        _frames[i]._frameDescriptors.init(_device, 1000, frame_sizes);

        _mainDeletionQueue.push_function(
            [&, i]()
            {
                // get_current_frame()._deletionQueue.flush();
                _frames[i]._frameDescriptors.clear_pools(_device);
                _frames[i]._frameDescriptors.destroy_pools(_device);
            });
    }
}

void VulkanEngine::init_vulkan()
{
    vkb::InstanceBuilder builder;

    // enable Debug Printf
    VkValidationFeatureEnableEXT enables[] = { VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT };
    VkValidationFeaturesEXT vfeat{
        VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT, // sType
        nullptr,                                   // pNext
        (uint32_t)std::size(enables),
        enables, // enabled*
        0,
        nullptr // disabled*
    };

    // make the vulkan instance, with basic debug features
    // auto inst_ret = builder.set_app_name("Example Vulkan Application")
    //     .request_validation_layers(bUseValidationLayers)
    //     .enable_extension(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME)
    //     .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)
    //     .add_debug_messenger_severity( // printf shows up as INFO
    //         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
    //         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    //         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    //     .use_default_debug_messenger()
    //     .require_api_version(1, 3, 0)
    //     .build();
    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                        .request_validation_layers(bUseValidationLayers)
                        .use_default_debug_messenger()
                        .require_api_version(1, 3, 0)
                        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // grab the instance
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(_window, _instance, nullptr, &_surface);

    // vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features.dynamicRendering = true;
    features.synchronization2 = true;

    // vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    // vulkan 1.1 features — `shaderDrawParameters` is needed because Slang
    // emits the SPIR-V DrawParameters capability on every vertex shader
    // (it backs SV_VertexID/SV_InstanceID with the base-vertex/instance
    // offset). glslang doesn't emit it unless the shader uses gl_DrawID etc.,
    // which is why the GLSL build worked without this.
    VkPhysicalDeviceVulkan11Features features11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    features11.shaderDrawParameters = true;

    // VK_EXT_mesh_shader features — feeds the optional mesh-shader rendering path.
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
    meshShaderFeatures.meshShader = VK_TRUE;
    meshShaderFeatures.taskShader = VK_TRUE;

    // VK_KHR_acceleration_structure + VK_KHR_ray_query — feeds the raytraced
    // shadow path. Ray query (vs full RT pipeline) is sufficient because the
    // shadow lookup happens inside the existing fragment shaders.
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeat{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    asFeat.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceRayQueryFeaturesKHR rqFeat{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
    rqFeat.rayQuery = VK_TRUE;

    // use vkbootstrap to select a gpu.
    // We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 3)
                                             .set_required_features_13(features)
                                             .set_required_features_12(features12)
                                             .set_required_features_11(features11)
                                             .add_required_extension(VK_EXT_MESH_SHADER_EXTENSION_NAME)
                                             .add_required_extension_features(meshShaderFeatures)
                                             .add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
                                             .add_required_extension(VK_KHR_RAY_QUERY_EXTENSION_NAME)
                                             .add_required_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
                                             .add_required_extension_features(asFeat)
                                             .add_required_extension_features(rqFeat)
                                             .set_surface(_surface)
                                             .select()
                                             .value();

    // create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{ physicalDevice };

    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    _device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    pfnCmdDrawMeshTasksEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(
        vkGetDeviceProcAddr(_device, "vkCmdDrawMeshTasksEXT"));
    assert(pfnCmdDrawMeshTasksEXT && "vkCmdDrawMeshTasksEXT not available");

    // Ray-tracing acceleration-structure entry points. vk-bootstrap does not
    // surface these; load them manually like vkCmdDrawMeshTasksEXT above.
    pfnCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(_device, "vkCreateAccelerationStructureKHR"));
    pfnDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(_device, "vkDestroyAccelerationStructureKHR"));
    pfnCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(_device, "vkCmdBuildAccelerationStructuresKHR"));
    pfnGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(_device, "vkGetAccelerationStructureBuildSizesKHR"));
    pfnGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(_device, "vkGetAccelerationStructureDeviceAddressKHR"));
    assert(pfnCreateAccelerationStructureKHR && "VK_KHR_acceleration_structure entry points not available");
    assert(pfnDestroyAccelerationStructureKHR);
    assert(pfnCmdBuildAccelerationStructuresKHR);
    assert(pfnGetAccelerationStructureBuildSizesKHR);
    assert(pfnGetAccelerationStructureDeviceAddressKHR);

    // Capture scratch-buffer alignment required by AS builds.
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
    VkPhysicalDeviceProperties2 props2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    props2.pNext = &asProps;
    vkGetPhysicalDeviceProperties2(_chosenGPU, &props2);
    _asScratchAlignment = std::max<VkDeviceSize>(asProps.minAccelerationStructureScratchOffsetAlignment, 256);

    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosenGPU;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    /*_mainDeletionQueue.push_function([&]() {
        vmaDestroyAllocator(_allocator);
        });*/
}
void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU, _device, _surface };

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            //.use_default_format_selection()
            .set_desired_format(
                VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
            // use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();

    _swapchainExtent = vkbSwapchain.extent;
    // store swapchain and its related images
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::init_swapchain()
{
    create_swapchain(_windowExtent.width, _windowExtent.height);

    // draw image size will match the window
    VkExtent3D drawImageExtent = { _windowExtent.width, _windowExtent.height, 1 };

    // hardcoding the draw format to 32 bit float
    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

    // for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    // vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);
    VK_CHECK(
        vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr));
    vmaSetAllocationName(_allocator, _drawImage.allocation, "DrawImage");
    fmt::print("Created image: DrawImage\n");

    // build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info =
        vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);

    // allocate and create the image
    VK_CHECK(
        vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr));
    vmaSetAllocationName(_allocator, _depthImage.allocation, "DepthImage");
    fmt::print("Created image: DepthImage\n");

    // build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo dview_info =
        vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));

    auto drawImage = _drawImage;
    auto depthImage = _depthImage;
    auto device = _device;
    auto allocator = _allocator;
    // add to deletion queues
    _mainDeletionQueue.push_function(
        [drawImage, depthImage, device, allocator]()
        {
            VmaAllocationInfo info{};
            vmaGetAllocationInfo(allocator, drawImage.allocation, &info);
            fmt::print("Destroying image: {0}\n", (info.pName ? info.pName : "unnamed image"));
            vkDestroyImageView(device, drawImage.imageView, nullptr);
            vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);

            vmaGetAllocationInfo(allocator, depthImage.allocation, &info);
            fmt::print("Destroying image: {0}\n", (info.pName ? info.pName : "unnamed image"));
            vkDestroyImageView(device, depthImage.imageView, nullptr);
            vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
        });
}
void VulkanEngine::init_commands()
{
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    /*VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = nullptr;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = _graphicsQueueFamily;*/

    VkCommandPoolCreateInfo commandPoolInfo =
        vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {

        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        //// allocate the default command buffer that we will use for rendering
        // VkCommandBufferAllocateInfo cmdAllocInfo = {};
        // cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        // cmdAllocInfo.pNext = nullptr;
        // cmdAllocInfo.commandPool = _frames[i]._commandPool;
        // cmdAllocInfo.commandBufferCount = 1;
        // cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        //  allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
    }

    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

    // allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

    _mainDeletionQueue.push_function([=]() { vkDestroyCommandPool(_device, _immCommandPool, nullptr); });
}
void VulkanEngine::init_sync_structures()
{
    // create syncronization structures
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));
    }

    // build per-image present semaphores (swapchain already created)
    rebuild_present_semaphores();

    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
    _mainDeletionQueue.push_function([=]() { vkDestroyFence(_device, _immFence, nullptr); });
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void VulkanEngine::init_imgui()
{
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                                          { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                                          { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                                          { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                                          { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 18.0f);

    // this initializes imgui for SDL
    ImGui_ImplSDL3_InitForVulkan(_window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _chosenGPU;
    init_info.Device = _device;
    init_info.Queue = _graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    // dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    // add the destroy the imgui created structures
    _mainDeletionQueue.push_function(
        [=]()
        {
            ImGui_ImplVulkan_Shutdown();
            vkDestroyDescriptorPool(_device, imguiPool, nullptr);
        });
}

void VulkanEngine::cleanup()
{
    if (_isInitialized)
    {

        // make sure the gpu has stopped doing its things
        vkDeviceWaitIdle(_device);
        loadedNodes.clear();
        loadedScenes.clear();
        for (auto& mesh : testMeshes)
        {
            destroy_blas(mesh->meshBuffers);
            destroy_buffer(mesh->meshBuffers.indexBuffer);
            destroy_buffer(mesh->meshBuffers.vertexBuffer);
            destroy_buffer(mesh->meshBuffers.instanceTransformBuffer);
            if (mesh->meshBuffers.meshletCount > 0)
            {
                destroy_buffer(mesh->meshBuffers.meshletBuffer);
                destroy_buffer(mesh->meshBuffers.meshletVerticesBuffer);
                destroy_buffer(mesh->meshBuffers.meshletTrianglesBuffer);
                destroy_buffer(mesh->meshBuffers.meshletBoundsBuffer);
            }
        }
        testMeshes.clear();

        _groundNode.reset(); // buffers destroyed via testMeshes loop above

        for (int i = 0; i < FRAME_OVERLAP; i++)
        {
            _frames[i]._deletionQueue.flush();
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
            // destroy sync objects
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
        }

        // get_current_frame()._deletionQueue.flush();
        metalRoughMaterial.clear_resources(_device);

        // flush the global deletion queue
        _mainDeletionQueue.flush();
        fmt::print("loadedScenes={}, loadedNodes={}, testMeshes={}\n",
                   loadedScenes.size(),
                   loadedNodes.size(),
                   testMeshes.size());

        char* stats = nullptr;
        vmaBuildStatsString(_allocator, &stats, VK_TRUE);
        fmt::print("{}\n", stats ? stats : "<no stats>");
        vmaFreeStatsString(_allocator, stats);

        vmaDestroyAllocator(_allocator); // explicit, after queue flush

        destroy_swapchain();

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);

        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);
        if (_controller)
        {
            SDL_CloseGamepad(_controller);
            _controller = nullptr;
        }
        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::destroy_swapchain()
{
    // destroy per-image semaphores first
    for (auto s : _presentSemaphores)
        vkDestroySemaphore(_device, s, nullptr);
    _presentSemaphores.clear();

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    // destroy swapchain resources
    for (int i = 0; i < _swapchainImageViews.size(); i++)
    {

        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }
}

void VulkanEngine::draw()
{
    update_scene();

    // wait until the gpu has finished rendering the last frame. Timeout of 1
    // second
    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));
    get_current_frame()._deletionQueue.flush();
    get_current_frame()._frameDescriptors.clear_pools(_device);

    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    // request image from the swapchain
    uint32_t swapchainImageIndex = 0;
    VK_CHECK(vkAcquireNextImageKHR(
        _device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex));

    // naming it cmd for shorter writing
    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know
    // that
    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    ////OLD Command Buffer REcording
    ////start the command buffer recording
    // VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    ////make the swapchain image into writeable mode before rendering
    // vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
    // VK_IMAGE_LAYOUT_GENERAL);

    ////make a clear-color from frame number. This will flash with a 120 frame period.
    // VkClearColorValue clearValue;
    // float flash = std::abs(std::sin(_frameNumber / 120.0f));
    // clearValue = { { flash, 0.0f, 0.0f, 1.0f } };

    // VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    ////clear image
    // vkCmdClearColorImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1,
    // &clearRange);

    ////make the swapchain image into presentable mode
    // vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL,
    // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    ////finalize the command buffer (we can no longer add commands, but it can now be executed)
    // VK_CHECK(vkEndCommandBuffer(cmd));

    _drawExtent.width = _drawImage.imageExtent.width;
    _drawExtent.height = _drawImage.imageExtent.height;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    draw_background(cmd);

    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transition_image(
        cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    update_transform(cmd);

    if (_useRaytracedShadows)
    {
        // Raytraced-shadow path: build a fresh TLAS over the just-animated
        // instance buffer and let the fragment shaders sample it via ray query.
        // The rasterized shadow map is skipped entirely. We still need the
        // shadow image to be in SHADER_READ_ONLY_OPTIMAL for the descriptor
        // write — its contents are never sampled when shadowMode = 1.
        vkutil::transition_image(cmd,
                                 _shadowImage.image,
                                 VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 VK_IMAGE_ASPECT_DEPTH_BIT);
        build_tlas(cmd);
    }
    else
    {
        // Rasterized-shadow path. Bind the placeholder TLAS so the descriptor
        // at binding 2 is always valid even though no ray-query is issued.
        _tlas = _placeholderTlas;

        // Render the directional shadow map after instance transforms are written
        // (the shadow vertex shader reads the same instance transform buffer the
        // camera pass uses) and before the lighting passes that sample it.
        draw_shadow(cmd);
    }

    draw_geometry(cmd);

    // transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(
        cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(
        cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(
        cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

    // set swapchain image layout to Attachment Optimal so we can draw it
    vkutil::transition_image(cmd,
                             _swapchainImages[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // draw imgui into the swapchain image
    draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

    // set swapchain image layout to Present so we can draw it
    vkutil::transition_image(cmd,
                             _swapchainImages[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    //// set swapchain image layout to Present so we can show it on the screen
    // vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    ////finalize the command buffer (we can no longer add commands, but it can now be executed)
    // VK_CHECK(vkEndCommandBuffer(cmd));

    // prepare the submission to the queue.
    // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    // we will signal the _renderSemaphore, to signal that rendering has finished

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    // VkSemaphoreSubmitInfo waitInfo =
    // vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
    // get_current_frame()._swapchainSemaphore); VkSemaphoreSubmitInfo signalInfo =
    // vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);
    VkSemaphoreSubmitInfo waitInfo =
        vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_TRANSFER_BIT, // first real usage of the swapchain image
                                      get_current_frame()._swapchainSemaphore);

    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,     // finished all rendering affecting the image
        _presentSemaphores[swapchainImageIndex]); // per-image present semaphore

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

    // prepare present
    //  this will put the image we just rendered to into the visible window.
    //  we want to wait on the _renderSemaphore for that,
    //  as its necessary that drawing commands have finished before the image is displayed to the user

    // VkPresentInfoKHR presentInfo = {};
    // presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    // presentInfo.pNext = nullptr;
    // presentInfo.pSwapchains = &_swapchain;
    // presentInfo.swapchainCount = 1;

    // presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
    // presentInfo.waitSemaphoreCount = 1;

    // presentInfo.pImageIndices = &swapchainImageIndex;

    VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    VkSemaphore presentWait = _presentSemaphores[swapchainImageIndex];
    presentInfo.pWaitSemaphores = &presentWait;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    // increase the number of frames drawn
    _frameNumber++;
}

void VulkanEngine::rebuild_present_semaphores()
{
    // destroy old if any
    for (auto s : _presentSemaphores)
        if (s)
            vkDestroySemaphore(_device, s, nullptr);
    _presentSemaphores.clear();

    // build new, sized to current swapchain image count
    _presentSemaphores.resize(_swapchainImages.size());
    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (auto& s : _presentSemaphores)
    {
        VK_CHECK(vkCreateSemaphore(_device, &sci, nullptr, &s));
    }
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
    // begin a render pass  connected to our draw image
    VkRenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment =
        vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, &depthAttachment);
    // VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &renderInfo);

    // DRAW TRIANGLE
    // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);
    // set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = _drawExtent.width;
    viewport.height = _drawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = _drawExtent.width;
    scissor.extent.height = _drawExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    ////launch a draw command to draw 3 vertices
    // vkCmdDraw(cmd, 3, 1, 0, 0);

    // launch a draw command to draw 3 vertices
    // vkCmdDraw(cmd, 3, 1, 0, 0);

    // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

    // DRAW Monkey
    ////bind a texture
    // VkDescriptorSet imageSet = get_current_frame()._frameDescriptors.allocate(_device, _singleImageDescriptorLayout);
    //{
    //     DescriptorWriter writer;
    //     writer.write_image(0, _errorCheckerboardImage.imageView, _defaultSamplerNearest,
    //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    //    writer.update_set(_device, imageSet);
    //}

    // vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipelineLayout, 0, 1, &imageSet, 0, nullptr);

    // GPUDrawPushConstants push_constants;
    // push_constants.worldMatrix = glm::mat4{ 1.f };
    // push_constants.vertexBuffer = rectangle.vertexBufferAddress;

    // glm::mat4 view = glm::translate(glm::vec3{ 0,0,-5 });
    //// camera projection
    // glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)_drawExtent.width / (float)_drawExtent.height,
    // 10000.f, 0.1f);

    //// invert the Y direction on projection matrix so that we are more similar
    //// to opengl and gltf axis
    // projection[1][1] *= -1;

    // push_constants.worldMatrix = projection * view;

    // vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants),
    // &push_constants); vkCmdBindIndexBuffer(cmd, rectangle.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    // vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

    // Draw monkey 2
    // view is for transform monkey 2
    // vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants),
    // &push_constants);
    /// vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

    // Setup Uniform Descriptor Set
    // allocate a new uniform buffer for the scene data
    AllocatedBuffer gpuSceneDataBuffer = create_buffer(
        sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "GPUSceneData");
    // add it to the deletion queue of this frame so it gets deleted once its been used
    get_current_frame()._deletionQueue.push_function([=, this]() { destroy_buffer(gpuSceneDataBuffer); });

    // write the buffer
    // GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
    //*sceneUniformData = sceneData;

    memcpy(gpuSceneDataBuffer.info.pMappedData, &sceneData, sizeof(GPUSceneData));
    vmaFlushAllocation(_allocator, gpuSceneDataBuffer.allocation, 0, VK_WHOLE_SIZE);

    // create a descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor =
        get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);

    DescriptorWriter writer;
    writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    // Shadow map sampled by mesh.frag / ground.frag. draw_shadow has already
    // transitioned the image to SHADER_READ_ONLY_OPTIMAL by the time this
    // descriptor set is consumed.
    writer.write_image(1,
                       _shadowImage.imageView,
                       _shadowSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    // Raytracing TLAS. _tlas is the placeholder when _useRaytracedShadows is
    // off; build_tlas overwrites it for the current frame when on.
    writer.write_as(2, _tlas);
    writer.update_set(_device, globalDescriptor);

    // push_constants.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAddress;

    // vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants),
    // &push_constants); vkCmdBindIndexBuffer(cmd, testMeshes[2]->meshBuffers.indexBuffer.buffer, 0,
    // VK_INDEX_TYPE_UINT32); vkCmdDrawIndexed(cmd, testMeshes[2]->surfaces[0].count, 1,
    // testMeshes[2]->surfaces[0].startIndex, 0, 0);

    // Loop over actors

    for (const RenderObject& draw : mainDrawContext.OpaqueSurfaces)
    {
        // Mesh-shader path applies only to Suzanne's opaque pipeline; ground &
        // transparent surfaces always take the vertex path.
        const bool useMeshShaders = _useMeshShaders
                                 && draw.material->pipeline == &metalRoughMaterial.opaquePipeline
                                 && draw.meshletCount > 0
                                 && draw.instanceCount > 0;

        MaterialPipeline* pp = useMeshShaders
                                 ? &metalRoughMaterial.opaqueMeshPipeline
                                 : draw.material->pipeline;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pp->pipeline);
        vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pp->layout, 0, 1, &globalDescriptor, 0, nullptr);
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pp->layout,
                                1,
                                1,
                                &draw.material->materialSet,
                                0,
                                nullptr);

        if (useMeshShaders)
        {
            GPUMeshShaderPushConstants pc{};
            pc.worldMatrix = draw.transform;
            pc.vertexBuffer = draw.vertexBufferAddress;
            pc.instanceTransformBuffer = draw.instanceTransformBufferAddress;
            pc.meshletBuffer = draw.meshletBufferAddress;
            pc.meshletVertices = draw.meshletVerticesAddress;
            pc.meshletTriangles = draw.meshletTrianglesAddress;
            pc.meshletBounds = draw.meshletBoundsAddress;
            pc.meshletCount = draw.meshletCount;
            pc.instanceCount = draw.instanceCount;

            vkCmdPushConstants(cmd,
                               pp->layout,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT,
                               0,
                               sizeof(pc),
                               &pc);

            // Task shader: workgroup size 32, each thread handles one meshlet.
            // Dispatch ceil(meshletCount / 32) task workgroups per instance; the
            // task shader compacts survivors and DispatchMesh()es the mesh stage.
            constexpr uint32_t kTaskGroupSize = 32;
            const uint32_t taskGroupsX = (draw.meshletCount + kTaskGroupSize - 1) / kTaskGroupSize;
            pfnCmdDrawMeshTasksEXT(cmd, taskGroupsX, draw.instanceCount, 1);
        }
        else
        {
            vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            GPUDrawPushConstants pushConstants;
            pushConstants.vertexBuffer = draw.vertexBufferAddress;
            pushConstants.instanceTransformBuffer = draw.instanceTransformBufferAddress;
            pushConstants.worldMatrix = draw.transform;
            vkCmdPushConstants(cmd,
                               pp->layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(GPUDrawPushConstants),
                               &pushConstants);

            vkCmdDrawIndexed(cmd, draw.indexCount, draw.instanceCount, draw.firstIndex, 0, 0);
        }
    }

    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw_background(VkCommandBuffer cmd)
{
    //--------------------------------------------------
    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];
    // ComputeEffect& effect = backgroundEffects[1];

    // bind the background compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

    vkCmdPushConstants(
        cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);
    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it

    if (currentBackgroundEffect == 2)
    {
        vkCmdDispatch(cmd, 1, 1, 1);
    }
    else
    {
        vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
    }
}

void VulkanEngine::draw_shadow(VkCommandBuffer cmd)
{
    // Per-frame UBO that the shadow vertex shader reads (set 0). Matches the
    // allocation pattern in draw_geometry — frame deletion queue tears it down.
    AllocatedBuffer shadowSceneBuffer = create_buffer(
        sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "ShadowSceneData");
    get_current_frame()._deletionQueue.push_function([=, this]() { destroy_buffer(shadowSceneBuffer); });
    memcpy(shadowSceneBuffer.info.pMappedData, &sceneData, sizeof(GPUSceneData));
    vmaFlushAllocation(_allocator, shadowSceneBuffer.allocation, 0, VK_WHOLE_SIZE);

    VkDescriptorSet shadowSceneSet =
        get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);
    {
        DescriptorWriter writer;
        writer.write_buffer(
            0, shadowSceneBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        // Bind the shadow image to its own sampler slot (binding 1) — the shadow
        // pass itself doesn't read it, but the descriptor layout requires every
        // binding to be populated. Use SHADER_READ_ONLY_OPTIMAL since that's the
        // only valid layout outside this pass; the depth image will be
        // transitioned to DEPTH_ATTACHMENT below before rendering.
        writer.write_image(1,
                           _shadowImage.imageView,
                           _shadowSampler,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        // TLAS binding is required by the layout even though the shadow caster
        // shaders don't consume it. The placeholder is sufficient here.
        writer.write_as(2, _placeholderTlas);
        writer.update_set(_device, shadowSceneSet);
    }

    // Transition the shadow image to depth-attachment layout for writing.
    vkutil::transition_image(cmd,
                             _shadowImage.image,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_ASPECT_DEPTH_BIT);

    VkRenderingAttachmentInfo depthAttachment =
        vkinit::depth_attachment_info(_shadowImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    // Reverse-Z: clear to 0 (= farthest from light).
    depthAttachment.clearValue.depthStencil = { 0.f, 0 };

    VkRenderingInfo renderInfo = vkinit::rendering_info(_shadowExtent, nullptr, &depthAttachment);
    // No color attachment.
    renderInfo.colorAttachmentCount = 0;
    renderInfo.pColorAttachments = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = (float)_shadowExtent.width;
    viewport.height = (float)_shadowExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = _shadowExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    const bool useMS = _useMeshShaders;
    VkPipeline       activePipeline = useMS ? _shadowMeshPipeline       : _shadowPipeline;
    VkPipelineLayout activeLayout   = useMS ? _shadowMeshPipelineLayout : _shadowPipelineLayout;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activeLayout, 0, 1, &shadowSceneSet, 0, nullptr);

    // Skip ground (flat plane, no meaningful shadow contribution + has no instance
    // transform buffer for the shadow.vert to read). Same filter as update_transform.
    VkDeviceAddress groundAddr =
        _groundNode ? _groundNode->mesh->meshBuffers.instanceTransformBufferAddress : 0;

    for (const RenderObject& draw : mainDrawContext.OpaqueSurfaces)
    {
        if (draw.instanceTransformBufferAddress == groundAddr)
            continue;

        if (useMS && draw.meshletCount > 0 && draw.instanceCount > 0)
        {
            GPUMeshShaderPushConstants pc{};
            pc.worldMatrix = draw.transform;
            pc.vertexBuffer = draw.vertexBufferAddress;
            pc.instanceTransformBuffer = draw.instanceTransformBufferAddress;
            pc.meshletBuffer = draw.meshletBufferAddress;
            pc.meshletVertices = draw.meshletVerticesAddress;
            pc.meshletTriangles = draw.meshletTrianglesAddress;
            pc.meshletBounds = draw.meshletBoundsAddress;
            pc.meshletCount = draw.meshletCount;
            pc.instanceCount = draw.instanceCount;
            vkCmdPushConstants(cmd,
                               activeLayout,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT,
                               0,
                               sizeof(pc),
                               &pc);
            // Shadow pass has no task shader — one mesh workgroup per (meshlet, instance).
            pfnCmdDrawMeshTasksEXT(cmd, draw.meshletCount, draw.instanceCount, 1);
        }
        else
        {
            vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            GPUDrawPushConstants pc;
            pc.worldMatrix = draw.transform;
            pc.vertexBuffer = draw.vertexBufferAddress;
            pc.instanceTransformBuffer = draw.instanceTransformBufferAddress;
            vkCmdPushConstants(
                cmd, activeLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

            vkCmdDrawIndexed(cmd, draw.indexCount, draw.instanceCount, draw.firstIndex, 0, 0);
        }
    }

    vkCmdEndRendering(cmd);

    // Hand the shadow image off to the lighting shaders for sampling.
    // Pass the depth aspect explicitly — the stock transition_image picks the
    // aspect from the new layout, and SHADER_READ_ONLY_OPTIMAL would default
    // to the color aspect, which would leave the depth aspect un-transitioned.
    vkutil::transition_image(cmd,
                             _shadowImage.image,
                             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanEngine::update_transform(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _updateTransformPipeline);

    float time = float(SDL_GetTicks()) / 1000.0f;

    VkDeviceAddress groundAddr = _groundNode ? _groundNode->mesh->meshBuffers.instanceTransformBufferAddress : 0;

    std::unordered_set<VkDeviceAddress> dispatched;
    for (const RenderObject& draw : mainDrawContext.OpaqueSurfaces)
    {
        if (draw.instanceTransformBufferAddress == groundAddr)
            continue; // ground is static
        if (!dispatched.insert(draw.instanceTransformBufferAddress).second)
            continue;

        const float padScale = 10.0f;
        UpdateTransformPushConstants pc{};
        pc.instanceTransformBuffer = draw.instanceTransformBufferAddress;
        pc.time = time;
        pc.count = draw.instanceCount;
        pc.padX = _padLeftAxis.x * padScale;
        pc.padY = -_padLeftAxis.y * padScale; // SDL Y is +down; flip so up-stick = +Y

        vkCmdPushConstants(cmd, _updateTransformPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
        vkCmdDispatch(cmd, (pc.count + 63) / 64, 1, 1);
    }

    VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    // The instance-transform buffer is consumed by either the vertex shader
    // (traditional path) or the mesh shader (mesh-shader path). Include both.
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT |
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         1,
                         &barrier,
                         0,
                         nullptr,
                         0,
                         nullptr);
}

// =============================================================================
// Raytraced shadows — VK_KHR_acceleration_structure + VK_KHR_ray_query.
//
// Pipeline:
//   build_blas (one-time per MeshAsset, called from loadGltfMeshes)
//     -> AllocatedBuffer::blasBuffer holds the AS storage
//     -> mb.blas, mb.blasAddress populated
//   init_raytracing (startup)
//     -> _placeholderTlas, _placeholderTlasBuffer (empty 0-instance TLAS)
//     -> _asScratchBuffer (grows on demand)
//     -> _tlas = _placeholderTlas
//   build_tlas (every frame when _useRaytracedShadows=true)
//     -> dispatch build_tlas_instances.comp to write VkASInstance records
//     -> vkCmdBuildAccelerationStructuresKHR builds a fresh top-level
//     -> previous _tlas + its buffer pushed to frame deletion queue
// =============================================================================

namespace {

inline VkDeviceSize align_up(VkDeviceSize v, VkDeviceSize a)
{
    return (v + a - 1) & ~(a - 1);
}

// Mirror of shaders/build_tlas_instances.comp.slang::BuildTlasPC.
struct BuildTlasInstancesPC
{
    VkDeviceAddress srcTransforms;            // 8
    VkDeviceAddress outInstances;             // 8
    uint32_t        blasLow;                  // 4
    uint32_t        blasHigh;                 // 4
    uint32_t        count;                    // 4
    uint32_t        outOffset;                // 4
    uint32_t        instanceCustomIndexBase;  // 4
    uint32_t        flagsAndMask;             // 4
};
static_assert(sizeof(BuildTlasInstancesPC) == 40,
              "BuildTlasInstancesPC must match shaders/build_tlas_instances.comp.slang");

} // namespace

void VulkanEngine::init_raytracing()
{
    // Persistent scratch buffer. Grows on demand inside build_blas / build_tlas.
    _asScratchBuffer = create_buffer(
        64 * 1024,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        "ASScratch");
    _mainDeletionQueue.push_function([this]() { destroy_buffer(_asScratchBuffer); });

    // Empty placeholder TLAS bound when raytraced shadows are disabled. The
    // ray query is also disabled in that mode so the contents are irrelevant —
    // we only need a valid descriptor handle.
    VkAccelerationStructureGeometryKHR geom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geom.geometry.instances.arrayOfPointers = VK_FALSE;
    geom.geometry.instances.data.deviceAddress = 0;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR build{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    build.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries = &geom;

    uint32_t maxPrimitiveCount = 0;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    pfnGetAccelerationStructureBuildSizesKHR(
        _device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build, &maxPrimitiveCount, &sizeInfo);

    const VkDeviceSize storageSize = std::max<VkDeviceSize>(sizeInfo.accelerationStructureSize, 256);
    _placeholderTlasBuffer = create_buffer(
        storageSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        "PlaceholderTLAS");

    VkAccelerationStructureCreateInfoKHR ci{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    ci.buffer = _placeholderTlasBuffer.buffer;
    ci.size = storageSize;
    ci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    VK_CHECK(pfnCreateAccelerationStructureKHR(_device, &ci, nullptr, &_placeholderTlas));

    // Grow scratch if needed for this empty build.
    const VkDeviceSize scratchNeeded = align_up(std::max<VkDeviceSize>(sizeInfo.buildScratchSize, 256),
                                                _asScratchAlignment);
    if (_asScratchBuffer.info.size < scratchNeeded)
    {
        destroy_buffer(_asScratchBuffer);
        _asScratchBuffer = create_buffer(
            scratchNeeded,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            "ASScratch");
    }

    VkBufferDeviceAddressInfo scratchAddrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                               nullptr,
                                               _asScratchBuffer.buffer };
    VkDeviceAddress scratchAddr = vkGetBufferDeviceAddress(_device, &scratchAddrInfo);
    scratchAddr = align_up(scratchAddr, _asScratchAlignment);

    build.dstAccelerationStructure = _placeholderTlas;
    build.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = 0;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    immediate_submit([&](VkCommandBuffer cmd)
                     { pfnCmdBuildAccelerationStructuresKHR(cmd, 1, &build, &pRange); });

    _tlas = _placeholderTlas;

    _mainDeletionQueue.push_function(
        [this]()
        {
            if (_placeholderTlas)
            {
                pfnDestroyAccelerationStructureKHR(_device, _placeholderTlas, nullptr);
                _placeholderTlas = VK_NULL_HANDLE;
            }
            destroy_buffer(_placeholderTlasBuffer);
        });
}

void VulkanEngine::init_tlas_instance_pipeline()
{
    VkPushConstantRange pushRange{};
    pushRange.offset = 0;
    pushRange.size = sizeof(BuildTlasInstancesPC);
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    layoutInfo.setLayoutCount = 0;
    VK_CHECK(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_tlasInstancePipelineLayout));

    VkShaderModule shader = VK_NULL_HANDLE;
    if (!vkutil::load_shader_module("../../shaders/build_tlas_instances.comp.spv", _device, &shader))
    {
        fmt::println("Error loading build_tlas_instances.comp.spv");
    }

    VkPipelineShaderStageCreateInfo stageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipeInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    pipeInfo.layout = _tlasInstancePipelineLayout;
    pipeInfo.stage = stageInfo;
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &_tlasInstancePipeline));

    vkDestroyShaderModule(_device, shader, nullptr);

    _mainDeletionQueue.push_function(
        [this]()
        {
            vkDestroyPipeline(_device, _tlasInstancePipeline, nullptr);
            vkDestroyPipelineLayout(_device, _tlasInstancePipelineLayout, nullptr);
        });
}

void VulkanEngine::build_blas(MeshAsset& mesh)
{
    GPUMeshBuffers& mb = mesh.meshBuffers;
    if (mb.indexCount == 0 || mb.vertexCount == 0) return;
    if (mb.blas != VK_NULL_HANDLE) return;

    const uint32_t primitiveCount = mb.indexCount / 3;

    VkAccelerationStructureGeometryKHR geom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geom.geometry.triangles.vertexData.deviceAddress = mb.vertexBufferAddress;
    geom.geometry.triangles.vertexStride = sizeof(Vertex);
    geom.geometry.triangles.maxVertex = mb.vertexCount > 0 ? mb.vertexCount - 1 : 0;
    geom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geom.geometry.triangles.indexData.deviceAddress = mb.indexBufferAddress;
    geom.geometry.triangles.transformData.deviceAddress = 0;

    VkAccelerationStructureBuildGeometryInfoKHR build{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    build.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    pfnGetAccelerationStructureBuildSizesKHR(
        _device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build, &primitiveCount, &sizeInfo);

    mb.blasBuffer = create_buffer(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        "BLAS");

    VkAccelerationStructureCreateInfoKHR ci{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    ci.buffer = mb.blasBuffer.buffer;
    ci.size = sizeInfo.accelerationStructureSize;
    ci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    VK_CHECK(pfnCreateAccelerationStructureKHR(_device, &ci, nullptr, &mb.blas));

    const VkDeviceSize scratchNeeded = align_up(sizeInfo.buildScratchSize, _asScratchAlignment);
    if (_asScratchBuffer.info.size < scratchNeeded)
    {
        destroy_buffer(_asScratchBuffer);
        _asScratchBuffer = create_buffer(
            scratchNeeded,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            "ASScratch");
    }

    VkBufferDeviceAddressInfo scratchAddrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                               nullptr,
                                               _asScratchBuffer.buffer };
    VkDeviceAddress scratchAddr = vkGetBufferDeviceAddress(_device, &scratchAddrInfo);
    scratchAddr = align_up(scratchAddr, _asScratchAlignment);

    build.dstAccelerationStructure = mb.blas;
    build.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = primitiveCount;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    immediate_submit([&](VkCommandBuffer cmd)
                     { pfnCmdBuildAccelerationStructuresKHR(cmd, 1, &build, &pRange); });

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    addrInfo.accelerationStructure = mb.blas;
    mb.blasAddress = pfnGetAccelerationStructureDeviceAddressKHR(_device, &addrInfo);
}

void VulkanEngine::destroy_blas(GPUMeshBuffers& mb)
{
    if (mb.blas)
    {
        pfnDestroyAccelerationStructureKHR(_device, mb.blas, nullptr);
        mb.blas = VK_NULL_HANDLE;
    }
    if (mb.blasBuffer.buffer)
    {
        destroy_buffer(mb.blasBuffer);
        mb.blasBuffer = {};
    }
    mb.blasAddress = 0;
}

void VulkanEngine::build_tlas(VkCommandBuffer cmd)
{
    // Gather unique (mesh, instance count) pairs from the current draw list,
    // mirroring update_transform's dedup so the same instance buffer drives
    // both update_transform and TLAS-instance writes. Skip meshes without a
    // BLAS (e.g. a glb that failed AS-input flag setup).
    struct MeshBatch
    {
        VkDeviceAddress srcTransforms;
        VkDeviceAddress blasAddress;
        uint32_t        count;
    };
    std::vector<MeshBatch> batches;
    batches.reserve(mainDrawContext.OpaqueSurfaces.size());
    std::unordered_set<VkDeviceAddress> seen;
    uint32_t totalInstances = 0;

    // Skip the ground — it's a single-instance static mesh whose vertex shader
    // ignores instanceTransformBuffer (uses pc.renderMatrix instead). Reading
    // transform 0 from that buffer would pick up stale random spawner data from
    // uploadMesh and stamp a ghost ground at the origin, which then shadows
    // every visible ground fragment. Matches the draw_shadow filter — the
    // rasterized path also excludes the ground from caster geometry.
    VkDeviceAddress groundAddr =
        _groundNode ? _groundNode->mesh->meshBuffers.instanceTransformBufferAddress : 0;

    for (const RenderObject& draw : mainDrawContext.OpaqueSurfaces)
    {
        if (draw.instanceTransformBufferAddress == groundAddr) continue;
        if (!seen.insert(draw.instanceTransformBufferAddress).second) continue;
        if (draw.instanceCount == 0) continue;
        // Need a BLAS for this mesh. Skip if none was built.
        if (draw.indexBuffer == VK_NULL_HANDLE) continue;
        // Look up BLAS via the index buffer match. We don't store the BLAS
        // address on RenderObject (kept slim for the hot draw path), so walk
        // the loaded meshes to find it.
        VkDeviceAddress blasAddr = 0;
        for (const auto& mesh : testMeshes)
        {
            if (mesh->meshBuffers.indexBuffer.buffer == draw.indexBuffer && mesh->meshBuffers.blasAddress)
            {
                blasAddr = mesh->meshBuffers.blasAddress;
                break;
            }
        }
        if (blasAddr == 0) continue;
        batches.push_back({ draw.instanceTransformBufferAddress, blasAddr, draw.instanceCount });
        totalInstances += draw.instanceCount;
    }

    if (totalInstances == 0)
    {
        // No real instances this frame — bind the placeholder so the descriptor
        // stays valid. Any prior frame's real TLAS is already scheduled for
        // destruction in its own frame's deletion queue.
        _tlas = _placeholderTlas;
        return;
    }

    // Per-frame instance buffer: one VkAccelerationStructureInstanceKHR (64 B) per
    // instance. Pushed into the frame deletion queue so it survives in-flight reads.
    constexpr VkDeviceSize kInstanceStride = 64;
    AllocatedBuffer instBuffer = create_buffer(
        totalInstances * kInstanceStride,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_GPU_ONLY,
        "TLASInstances");
    get_current_frame()._deletionQueue.push_function([this, instBuffer]() { destroy_buffer(instBuffer); });

    VkBufferDeviceAddressInfo instAddrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, instBuffer.buffer };
    const VkDeviceAddress instAddr = vkGetBufferDeviceAddress(_device, &instAddrInfo);

    // Dispatch instance-record compute. The barrier from update_transform's
    // tail (added above) already covers compute→compute on the source buffer.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _tlasInstancePipeline);

    uint32_t writeOffset = 0;
    for (const MeshBatch& b : batches)
    {
        BuildTlasInstancesPC pc{};
        pc.srcTransforms = b.srcTransforms;
        pc.outInstances = instAddr;
        pc.blasLow = static_cast<uint32_t>(b.blasAddress & 0xFFFFFFFFu);
        pc.blasHigh = static_cast<uint32_t>((b.blasAddress >> 32) & 0xFFFFFFFFu);
        pc.count = b.count;
        pc.outOffset = writeOffset;
        pc.instanceCustomIndexBase = writeOffset;
        // Low 8 bits = flags (CULL_DISABLE so two-sided geometry works regardless of winding).
        // Next 8 bits = mask (0xFF means "visible to all rays").
        pc.flagsAndMask = (uint32_t)VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR
                        | (0xFFu << 8u);
        vkCmdPushConstants(
            cmd, _tlasInstancePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
        vkCmdDispatch(cmd, (b.count + 63) / 64, 1, 1);
        writeOffset += b.count;
    }

    // Compute-write → AS-build-read barrier (sync2).
    {
        VkBufferMemoryBarrier2 bb{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
        bb.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        bb.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        bb.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        bb.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        bb.buffer = instBuffer.buffer;
        bb.offset = 0;
        bb.size = VK_WHOLE_SIZE;
        VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dep.bufferMemoryBarrierCount = 1;
        dep.pBufferMemoryBarriers = &bb;
        vkCmdPipelineBarrier2(cmd, &dep);
    }

    // Set up TLAS build geometry referencing the instance buffer just filled.
    VkAccelerationStructureGeometryKHR tlasGeom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlasGeom.geometry.instances.arrayOfPointers = VK_FALSE;
    tlasGeom.geometry.instances.data.deviceAddress = instAddr;
    tlasGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &tlasGeom;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    pfnGetAccelerationStructureBuildSizesKHR(
        _device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &totalInstances, &sizeInfo);

    AllocatedBuffer tlasStorage = create_buffer(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        "TLAS");

    VkAccelerationStructureCreateInfoKHR ci{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    ci.buffer = tlasStorage.buffer;
    ci.size = sizeInfo.accelerationStructureSize;
    ci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    VkAccelerationStructureKHR newTlas = VK_NULL_HANDLE;
    VK_CHECK(pfnCreateAccelerationStructureKHR(_device, &ci, nullptr, &newTlas));

    const VkDeviceSize scratchNeeded = align_up(sizeInfo.buildScratchSize, _asScratchAlignment);
    if (_asScratchBuffer.info.size < scratchNeeded)
    {
        // Resize is rare (only when instance count grows the worst case beyond
        // the current allocation). Use immediate_submit to flush in-flight work
        // before destroying the old scratch buffer.
        vkDeviceWaitIdle(_device);
        destroy_buffer(_asScratchBuffer);
        _asScratchBuffer = create_buffer(
            scratchNeeded,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            "ASScratch");
    }
    VkBufferDeviceAddressInfo scratchAddrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                               nullptr,
                                               _asScratchBuffer.buffer };
    VkDeviceAddress scratchAddr = vkGetBufferDeviceAddress(_device, &scratchAddrInfo);
    scratchAddr = align_up(scratchAddr, _asScratchAlignment);

    buildInfo.dstAccelerationStructure = newTlas;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = totalInstances;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    pfnCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    // AS-write → fragment-shader-read barrier.
    {
        VkMemoryBarrier2 mb{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
        mb.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        mb.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        mb.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        mb.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dep.memoryBarrierCount = 1;
        dep.pMemoryBarriers = &mb;
        vkCmdPipelineBarrier2(cmd, &dep);
    }

    // The new TLAS handle + its backing storage are owned by THIS frame's
    // deletion queue. They die when this slot is reused FRAME_OVERLAP frames
    // later — by then this frame's GPU work has completed (fence guarantees).
    get_current_frame()._deletionQueue.push_function(
        [this, newTlas, tlasStorage]()
        {
            pfnDestroyAccelerationStructureKHR(_device, newTlas, nullptr);
            AllocatedBuffer b = tlasStorage;
            destroy_buffer(b);
        });
    _tlas = newTlas;
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

// Always-on stats readout. UE analog: `stat fps` / `stat unit Frame`.
// ImGui maintains a 60-frame moving-average framerate in IO.Framerate, so no
// manual timing is needed. Drawn as a decoration-less, click-through overlay
// pinned to the top-left so it doesn't fight the "background" controls panel.
void VulkanEngine::draw_stats_overlay()
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
                           | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    ImGui::SetNextWindowPos(ImVec2(10.f, 10.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("Stats", nullptr, flags))
    {
        const float fps = io.Framerate;
        ImGui::Text("FPS:   %6.1f", fps);
        ImGui::Text("Frame: %6.2f ms", 1000.0f / (fps > 0.f ? fps : 1.f));
    }
    ImGui::End();
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit)
    {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_EVENT_QUIT)
                bQuit = true;

            if (e.type == SDL_EVENT_WINDOW_MINIMIZED)
            {
                stop_rendering = true;
            }
            if (e.type == SDL_EVENT_WINDOW_RESTORED)
            {
                stop_rendering = false;
            }

            // send SDL event to imgui for handling
            mainCamera.processSDLEvent(e);
            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        // do not draw if we are minimized
        if (stop_rendering)
        {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // poll gamepad left stick
        if (_controller)
        {
            const float deadzone = 8000.0f;
            const float maxRange = 32767.0f;
            auto apply = [&](Sint16 raw)
            {
                float v = (float)raw;
                if (std::abs(v) < deadzone)
                    return 0.f;
                float sign = v < 0 ? -1.f : 1.f;
                return sign * ((std::abs(v) - deadzone) / (maxRange - deadzone));
            };
            _padLeftAxis.x = apply(SDL_GetGamepadAxis(_controller, SDL_GAMEPAD_AXIS_LEFTX));
            _padLeftAxis.y = apply(SDL_GetGamepadAxis(_controller, SDL_GAMEPAD_AXIS_LEFTY));
        }
        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        // some imgui UI to test
        ImGui::ShowDemoWindow();
        draw_stats_overlay();
        if (ImGui::Begin("background"))
        {

            ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

            ImGui::Text("Selected effect: ", selected.name);

            ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);

            ImGui::Separator();
            ImGui::Checkbox("Mesh shaders (Suzanne)", &_useMeshShaders);
            ImGui::Checkbox("Debug cluster color", &_debugClusterColor);
            ImGui::Checkbox("Lit cluster color", &_debugClusterLit);
            ImGui::Checkbox("Raytraced shadows", &_useRaytracedShadows);
        }
        ImGui::End();

        ImGui::Render();

        // make imgui calculate internal draw structures
        ImGui::Render();

        draw();
    }
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize,
                                            VkBufferUsageFlags usage,
                                            VmaMemoryUsage memoryUsage,
                                            const char* debugName)
{
    // allocate buffer
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;

    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(
        _allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    if (debugName && newBuffer.allocation)
    {
        vmaSetAllocationName(_allocator, newBuffer.allocation, debugName);
    }

    return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer)
{
    if (buffer.buffer == VK_NULL_HANDLE || buffer.allocation == nullptr)
    {
        return;
    }

    VmaAllocationInfo info{};
    vmaGetAllocationInfo(_allocator, buffer.allocation, &info);

    /*if (info.pName)
    {
        fmt::print("Destroying buffer: {}\n", info.pName);
    }
    else
    {
        fmt::print("Destroying unnamed buffer\n");
    }*/

    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

const int MAX_INSTANCE_COUNT = 1000;
GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
    const size_t instanceTransformBufferSize = MAX_INSTANCE_COUNT * sizeof(InstanceTransform);

    GPUMeshBuffers newSurface;

    // create vertex buffer (also serves as AS build input for raytraced shadows)
    newSurface.vertexBuffer = create_buffer(vertexBufferSize,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                            VMA_MEMORY_USAGE_GPU_ONLY,
                                            "VertexBuffer");
    // find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                .buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressInfo);
    newSurface.vertexCount = static_cast<uint32_t>(vertices.size());

    // create InstanceTransformBuffer
    newSurface.instanceTransformBuffer =
        create_buffer(instanceTransformBufferSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY,
                      "InstanceTransformBuffer");
    // find the adress of the InstanceTransformBuffer
    VkBufferDeviceAddressInfo deviceAdressInstanceTransformInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                                 .buffer = newSurface.instanceTransformBuffer.buffer };
    newSurface.instanceTransformBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressInstanceTransformInfo);

    // create index buffer (also serves as AS build input + needs a device address)
    newSurface.indexBuffer = create_buffer(indexBufferSize,
                                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                           VMA_MEMORY_USAGE_GPU_ONLY,
                                           "IndexBuffer");
    VkBufferDeviceAddressInfo deviceAdressIndexInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                     .buffer = newSurface.indexBuffer.buffer };
    newSurface.indexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressIndexInfo);
    newSurface.indexCount = static_cast<uint32_t>(indices.size());

    std::vector<InstanceTransform> transforms;
    std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> unitDist{ 0.f, 1.f };
    std::uniform_real_distribution<float> angleDist{ 0.f, 6.28318530718f };

    for (int i = 0; i < MAX_INSTANCE_COUNT; i++)
    {
        const float z = unitDist(rng) * 2.f - 1.f;
        const float theta = angleDist(rng);
        const float radius = 5.f * std::cbrt(unitDist(rng));
        const float xyRadius = std::sqrt(1.f - z * z);
        const glm::vec3 position{
            radius * xyRadius * std::cos(theta),
            radius * xyRadius * std::sin(theta),
            radius * z
        };

        InstanceTransform t;
        glm::mat4 mat = glm::translate(position);
        t.transform = mat;
        transforms.push_back(t);
    }

    AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize + instanceTransformBufferSize,
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VMA_MEMORY_USAGE_CPU_ONLY,
                                            "StagingBuffer");
    void* data = staging.allocation->GetMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);
    // copy instance transfroms
    memcpy((char*)data + vertexBufferSize + indexBufferSize, transforms.data(), instanceTransformBufferSize);

    immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            VkBufferCopy vertexCopy{ 0 };
            vertexCopy.dstOffset = 0;
            vertexCopy.srcOffset = 0;
            vertexCopy.size = vertexBufferSize;

            vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

            VkBufferCopy indexCopy{ 0 };
            indexCopy.dstOffset = 0;
            indexCopy.srcOffset = vertexBufferSize;
            indexCopy.size = indexBufferSize;

            vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);

            VkBufferCopy transformCopy{ 0 };
            transformCopy.dstOffset = 0;
            transformCopy.srcOffset = vertexBufferSize + indexBufferSize;
            transformCopy.size = instanceTransformBufferSize;

            vkCmdCopyBuffer(cmd, staging.buffer, newSurface.instanceTransformBuffer.buffer, 1, &transformCopy);
        });

    destroy_buffer(staging);

    return newSurface;
}

void VulkanEngine::InitClusters(MeshAsset& mesh, std::span<Vertex> vertices, std::span<uint32_t> indices)
{
    constexpr size_t kMaxVerts = 64;
    constexpr size_t kMaxTris  = 124;
    // Non-zero cone_weight asks meshopt to pick triangle clusterings that produce tight
    // normal cones — required for the task shader's backface-cone culling to be useful.
    constexpr float  kConeWeight = 0.5f;

    if (vertices.empty() || indices.empty())
    {
        return;
    }

    const size_t bound = meshopt_buildMeshletsBound(indices.size(), kMaxVerts, kMaxTris);
    std::vector<meshopt_Meshlet> rawMeshlets(bound);
    std::vector<uint32_t>        meshletVertices(bound * kMaxVerts);
    std::vector<uint8_t>         meshletTrianglesU8(bound * kMaxTris * 3);

    const size_t meshletCount = meshopt_buildMeshlets(
        rawMeshlets.data(),
        meshletVertices.data(),
        meshletTrianglesU8.data(),
        indices.data(),
        indices.size(),
        &vertices[0].position.x,
        vertices.size(),
        sizeof(Vertex),
        kMaxVerts,
        kMaxTris,
        kConeWeight);

    if (meshletCount == 0)
    {
        return;
    }

    // Re-pack the byte triangle stream as one uint32 per triangle (low 24 bits used).
    // This avoids 8-bit storage requirements on the GPU and lets the shader unpack with
    // simple bit shifts.
    std::vector<uint32_t>         packedTriangles;
    std::vector<GpuMeshlet>       gpuMeshlets(meshletCount);
    std::vector<GpuMeshletBounds> gpuBounds(meshletCount);
    packedTriangles.reserve(meshletCount * kMaxTris);

    for (size_t i = 0; i < meshletCount; ++i)
    {
        const meshopt_Meshlet& m = rawMeshlets[i];
        gpuMeshlets[i] = {
            m.vertex_offset,
            static_cast<uint32_t>(packedTriangles.size()),
            m.vertex_count,
            m.triangle_count
        };

        const uint8_t* src = meshletTrianglesU8.data() + m.triangle_offset;
        for (uint32_t t = 0; t < m.triangle_count; ++t)
        {
            const uint32_t a = src[3 * t + 0];
            const uint32_t b = src[3 * t + 1];
            const uint32_t c = src[3 * t + 2];
            packedTriangles.push_back(a | (b << 8) | (c << 16));
        }

        // Bounding sphere + normal cone for this meshlet, in mesh-local space.
        const meshopt_Bounds b = meshopt_computeMeshletBounds(
            meshletVertices.data() + m.vertex_offset,
            meshletTrianglesU8.data() + m.triangle_offset,
            m.triangle_count,
            &vertices[0].position.x,
            vertices.size(),
            sizeof(Vertex));

        gpuBounds[i].centerRadius   = glm::vec4(b.center[0],    b.center[1],    b.center[2],    b.radius);
        gpuBounds[i].coneApex       = glm::vec4(b.cone_apex[0], b.cone_apex[1], b.cone_apex[2], 0.0f);
        gpuBounds[i].coneAxisCutoff = glm::vec4(b.cone_axis[0], b.cone_axis[1], b.cone_axis[2], b.cone_cutoff);
    }

    // Trim the vertex-index array to what was actually used.
    const meshopt_Meshlet& last = rawMeshlets[meshletCount - 1];
    meshletVertices.resize(last.vertex_offset + last.vertex_count);

    const size_t meshletBufferSize   = gpuMeshlets.size()     * sizeof(GpuMeshlet);
    const size_t mverticesBufferSize = meshletVertices.size() * sizeof(uint32_t);
    const size_t trianglesBufferSize = packedTriangles.size() * sizeof(uint32_t);
    const size_t boundsBufferSize    = gpuBounds.size()       * sizeof(GpuMeshletBounds);

    const VkBufferUsageFlags storageUsage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    mesh.meshBuffers.meshletBuffer =
        create_buffer(meshletBufferSize, storageUsage, VMA_MEMORY_USAGE_GPU_ONLY, "Meshlets");
    mesh.meshBuffers.meshletVerticesBuffer =
        create_buffer(mverticesBufferSize, storageUsage, VMA_MEMORY_USAGE_GPU_ONLY, "MeshletVertices");
    mesh.meshBuffers.meshletTrianglesBuffer =
        create_buffer(trianglesBufferSize, storageUsage, VMA_MEMORY_USAGE_GPU_ONLY, "MeshletTriangles");
    mesh.meshBuffers.meshletBoundsBuffer =
        create_buffer(boundsBufferSize, storageUsage, VMA_MEMORY_USAGE_GPU_ONLY, "MeshletBounds");

    {
        VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                        .buffer = mesh.meshBuffers.meshletBuffer.buffer };
        mesh.meshBuffers.meshletBufferAddress = vkGetBufferDeviceAddress(_device, &info);
    }
    {
        VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                        .buffer = mesh.meshBuffers.meshletVerticesBuffer.buffer };
        mesh.meshBuffers.meshletVerticesAddress = vkGetBufferDeviceAddress(_device, &info);
    }
    {
        VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                        .buffer = mesh.meshBuffers.meshletTrianglesBuffer.buffer };
        mesh.meshBuffers.meshletTrianglesAddress = vkGetBufferDeviceAddress(_device, &info);
    }
    {
        VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                        .buffer = mesh.meshBuffers.meshletBoundsBuffer.buffer };
        mesh.meshBuffers.meshletBoundsAddress = vkGetBufferDeviceAddress(_device, &info);
    }

    AllocatedBuffer staging = create_buffer(meshletBufferSize + mverticesBufferSize + trianglesBufferSize + boundsBufferSize,
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VMA_MEMORY_USAGE_CPU_ONLY,
                                            "MeshletStaging");
    void* data = staging.allocation->GetMappedData();
    char* cursor = static_cast<char*>(data);
    memcpy(cursor, gpuMeshlets.data(), meshletBufferSize);          cursor += meshletBufferSize;
    memcpy(cursor, meshletVertices.data(), mverticesBufferSize);    cursor += mverticesBufferSize;
    memcpy(cursor, packedTriangles.data(), trianglesBufferSize);    cursor += trianglesBufferSize;
    memcpy(cursor, gpuBounds.data(), boundsBufferSize);

    immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            VkBufferCopy copy{};
            copy.srcOffset = 0;
            copy.dstOffset = 0;
            copy.size = meshletBufferSize;
            vkCmdCopyBuffer(cmd, staging.buffer, mesh.meshBuffers.meshletBuffer.buffer, 1, &copy);

            copy.srcOffset = meshletBufferSize;
            copy.size = mverticesBufferSize;
            vkCmdCopyBuffer(cmd, staging.buffer, mesh.meshBuffers.meshletVerticesBuffer.buffer, 1, &copy);

            copy.srcOffset = meshletBufferSize + mverticesBufferSize;
            copy.size = trianglesBufferSize;
            vkCmdCopyBuffer(cmd, staging.buffer, mesh.meshBuffers.meshletTrianglesBuffer.buffer, 1, &copy);

            copy.srcOffset = meshletBufferSize + mverticesBufferSize + trianglesBufferSize;
            copy.size = boundsBufferSize;
            vkCmdCopyBuffer(cmd, staging.buffer, mesh.meshBuffers.meshletBoundsBuffer.buffer, 1, &copy);
        });

    destroy_buffer(staging);

    mesh.meshBuffers.meshletCount = static_cast<uint32_t>(meshletCount);

    fmt::println("InitClusters: mesh '{}' -> {} meshlets ({} verts, {} packed tris, {} bounds)",
                 mesh.name,
                 meshletCount,
                 meshletVertices.size(),
                 packedTriangles.size(),
                 gpuBounds.size());
}

void VulkanEngine::init_mesh_pipeline()
{
    VkShaderModule triangleFragShader;
    if (!vkutil::load_shader_module("../../shaders/tex_image.frag.spv", _device, &triangleFragShader))
    {
        fmt::print("Error when building the triangle fragment shader module (tex_image.frag.spv)\n");
    }
    else
    {
        fmt::print("Triangle fragment shader succesfully loaded (tex_image.frag.spv) \n");
    }

    VkShaderModule triangleVertexShader;
    if (!vkutil::load_shader_module("../../shaders/colored_triangle_mesh.vert.spv", _device, &triangleVertexShader))
    {
        fmt::print("Error when building the triangle vertex shader module (colored_triangle_mesh.vert.spv)\n");
    }
    else
    {
        fmt::print("Triangle vertex shader succesfully loaded (colored_triangle_mesh.vert.spv)\n");
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &bufferRange;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pSetLayouts = &_singleImageDescriptorLayout;
    pipeline_layout_info.setLayoutCount = 1;
    VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));

    PipelineBuilder pipelineBuilder;

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = _meshPipelineLayout;
    // connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
    // it will draw triangles
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    // filled triangles
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    // no backface culling
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    // no multisampling
    pipelineBuilder.set_multisampling_none();
    // no blending
    pipelineBuilder.disable_blending();
    // pipelineBuilder.enable_blending_additive();
    // pipelineBuilder.enable_blending_alphablend();

    // pipelineBuilder.disable_depthtest();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    // connect the image format we will draw into, from draw image
    pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(_depthImage.imageFormat);

    // finally build the pipeline
    _meshPipeline = pipelineBuilder.build_pipeline(_device);

    // clean structures
    vkDestroyShaderModule(_device, triangleFragShader, nullptr);
    vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

    _mainDeletionQueue.push_function(
        [&]()
        {
            vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
            vkDestroyPipeline(_device, _meshPipeline, nullptr);
        });
}

AllocatedImage VulkanEngine::create_image(
    VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, const char* debugName)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped)
    {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT)
    {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));
    if (debugName && newImage.allocation)
    {
        vmaSetAllocationName(_allocator, newImage.allocation, debugName);
    }
    return newImage;
}

AllocatedImage VulkanEngine::create_image(
    void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, const char* debugName)
{
    size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer =
        create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "UploadBuffer");

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = create_image(
        size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped, debugName);

    immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            vkutil::transition_image(
                cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = size;

            // copy the buffer into the image
            vkCmdCopyBufferToImage(
                cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            vkutil::transition_image(
                cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });

    destroy_buffer(uploadbuffer);
    if (debugName && new_image.allocation)
    {
        vmaSetAllocationName(_allocator, new_image.allocation, debugName);
    }
    return new_image;
}

void VulkanEngine::destroy_image(const AllocatedImage& img)
{
    VmaAllocationInfo info{};
    vmaGetAllocationInfo(_allocator, img.allocation, &info);

    /* if (info.pName)
     {
         fmt::print("Destroying image: {}\n", info.pName);
     }
     else
     {
         fmt::print("Destroying unnamed image\n");
     }*/

    vkDestroyImageView(_device, img.imageView, nullptr);
    vmaDestroyImage(_allocator, img.image, img.allocation);
}

void VulkanEngine::update_scene()
{
    mainDrawContext.OpaqueSurfaces.clear();

    mainCamera.update();

    glm::mat4 view = mainCamera.getViewMatrix();

    // camera projection
    glm::mat4 projection =
        glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.f, 0.1f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;

    sceneData.view = view;
    // sceneData.view = glm::translate(glm::vec3{ 0,0,-5 });

    sceneData.proj = projection;
    sceneData.viewproj = projection * sceneData.view;

    // sceneData.view = glm::translate(glm::vec3{ 0,0,-5 });
    //// camera projection
    // sceneData.proj = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height,
    // 10000.f, 0.1f);

    //// invert the Y direction on projection matrix so that we are more similar
    //// to opengl and gltf axis
    // sceneData.proj[1][1] *= -1;
    // sceneData.viewproj = sceneData.proj * sceneData.view;

    // some default lighting parameters
    //  Hemispheric ambient: cool sky overhead, warm ground beneath.
    sceneData.ambientColor = glm::vec4(0.45f, 0.55f, 0.75f, 1.f); // sky
    sceneData.groundColor = glm::vec4(0.20f, 0.18f, 0.15f, 1.f);  // ground
    sceneData.sunlightColor = glm::vec4(1.f, 0.97f, 0.92f, 1.f);
    sceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);
    sceneData.cameraPos = glm::vec4(mainCamera.position, 1.f);
    // .x picks the shadow path in mesh.frag / ground.frag (computeShadow).
    sceneData.shadowParams = glm::vec4(_useRaytracedShadows ? 1.f : 0.f, 0.f, 0.f, 0.f);
    // Mesh-shader debug view (cluster-color / lit-cluster-color). Lives in the
    // UBO so the regular fragment pipeline can read it without push-constant
    // stage/range issues.
    sceneData.debugParams =
        glm::vec4(_debugClusterColor ? 1.f : 0.f, _debugClusterLit ? 1.f : 0.f, 0.f, 0.f);

    // Shadow caster view-projection from the sun's perspective. Hand-tuned ortho
    // box covering the suzanne instance cloud near origin (radius ~5) plus the
    // ground patch directly behind it where shadows are cast (~50 units along
    // the light's anti-direction). A scene-AABB-driven fit is a follow-up.
    {
        glm::vec3 lightDir = glm::normalize(glm::vec3(sceneData.sunlightDirection));
        glm::vec3 lightPos = lightDir * 150.f;
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
        // Reverse-Z: pass near in the position normally reserved for far so that
        // gl_Position.z grows with proximity to the light (matches the camera pass).
        glm::mat4 lightProj = glm::ortho(-60.f, 60.f, -60.f, 60.f, 300.f, 1.f);
        // Vulkan clip-space Y matches the inverted camera projection above.
        lightProj[1][1] *= -1;
        sceneData.lightViewProj = lightProj * lightView;
    }

    //  Keep a vector of Actors that have transform, and a pointer to loaded node

    const double now = SDL_GetTicks() / 1000.0;
    if (_monkeyInstanceCount == 0)
    {
        _monkeyInstanceCount = 1;
        _lastMonkeySpawnTime = now;
    }
    else
    {
        while (_monkeyInstanceCount < MAX_INSTANCE_COUNT && now - _lastMonkeySpawnTime >= 1.0)
        {
            _monkeyInstanceCount++;
            _lastMonkeySpawnTime += 1.0;
        }
    }

    glm::mat4 T;
    loadedNodes["Suzanne"]->Draw(T, mainDrawContext, _monkeyInstanceCount);

    DrawGround(glm::translate(glm::mat4{ 1.f }, glm::vec3(0.f, -10.f, 0.f)));

    // for (size_t i = 0; i < 10; i++)
    //{
    //     for (size_t j = 0; j < 10; j++)
    //     {
    //         glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(i * 2, j * 2.0, 0.0f));
    //         loadedNodes["Suzanne"]->Draw(T, mainDrawContext);
    //     }
    // }

    // loadedNodes["Suzanne"]->Draw(glm::mat4{ 1.f }, mainDrawContext);

    // glm::mat4 T2 = glm::translate(glm::mat4(1.0f), glm::vec3(-4.0f, 0.0f, 0.0f));
    // loadedNodes["Suzanne"]->Draw(T2, mainDrawContext);

    // for (int x = -3; x < 3; x++) {

    //    glm::mat4 scale = glm::scale(glm::vec3{ 0.2 });
    //    glm::mat4 translation = glm::translate(glm::vec3{ x, 1, 0 });

    //    loadedNodes["Cube"]->Draw(translation * scale, mainDrawContext);
    //}

    // loadedScenes["structure"]->Draw(glm::mat4{ 1.f }, mainDrawContext);

    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

    double seconds_since_start = SDL_GetTicks() / 1000.0;

    effect.data.data4.r = seconds_since_start;
}

void GLTFMetallic_Roughness::build_pipelines(VulkanEngine* engine)
{
    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module("../../shaders/mesh.frag.spv", engine->_device, &meshFragShader))
    {
        fmt::println("Error when building the triangle fragment shader module");
    }

    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module("../../shaders/mesh.vert.spv", engine->_device, &meshVertexShader))
    {
        fmt::println("Error when building the triangle vertex shader module");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    materialLayout = layoutBuilder.build(engine->_device,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = { engine->_gpuSceneDataDescriptorLayout, materialLayout };

    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount = 2;
    mesh_layout_info.pSetLayouts = layouts;
    mesh_layout_info.pPushConstantRanges = &matrixRange;
    mesh_layout_info.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    // build the stage-create-info for both vertex and fragment stages. This lets
    // the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    // render format
    pipelineBuilder.set_color_attachment_format(engine->_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(engine->_depthImage.imageFormat);

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = newLayout;

    // finally build the pipeline
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

    // create the transparent variant
    pipelineBuilder.enable_blending_additive();

    pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

    vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);

    // --- Mesh-shader variant of the opaque pipeline.
    VkShaderModule meshMeshShader;
    if (!vkutil::load_shader_module("../../shaders/mesh.mesh.spv", engine->_device, &meshMeshShader))
    {
        fmt::println("Error loading mesh.mesh.spv");
    }

    // Phase 2: task shader for per-meshlet frustum + backface-cone culling.
    VkShaderModule meshTaskShader = VK_NULL_HANDLE;
    if (!vkutil::load_shader_module("../../shaders/mesh.task.spv", engine->_device, &meshTaskShader))
    {
        fmt::println("Error loading mesh.task.spv");
    }

    VkPushConstantRange meshShaderPushRange{};
    meshShaderPushRange.offset = 0;
    meshShaderPushRange.size = sizeof(GPUMeshShaderPushConstants);
    // Task and mesh stages read the buffer addresses. Cluster-debug flags now
    // live in sceneData (UBO), so the fragment stage no longer needs push access.
    meshShaderPushRange.stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT;

    VkPipelineLayoutCreateInfo meshShaderLayoutInfo = vkinit::pipeline_layout_create_info();
    meshShaderLayoutInfo.setLayoutCount = 2;
    meshShaderLayoutInfo.pSetLayouts = layouts;
    meshShaderLayoutInfo.pPushConstantRanges = &meshShaderPushRange;
    meshShaderLayoutInfo.pushConstantRangeCount = 1;

    VkPipelineLayout meshShaderLayout;
    VK_CHECK(vkCreatePipelineLayout(engine->_device, &meshShaderLayoutInfo, nullptr, &meshShaderLayout));
    opaqueMeshPipeline.layout = meshShaderLayout;

    PipelineBuilder meshShaderBuilder;
    meshShaderBuilder.set_mesh_shaders(meshTaskShader, meshMeshShader, meshFragShader);
    meshShaderBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    meshShaderBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    meshShaderBuilder.set_multisampling_none();
    meshShaderBuilder.disable_blending();
    meshShaderBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    meshShaderBuilder.set_color_attachment_format(engine->_drawImage.imageFormat);
    meshShaderBuilder.set_depth_format(engine->_depthImage.imageFormat);
    meshShaderBuilder._pipelineLayout = meshShaderLayout;

    opaqueMeshPipeline.pipeline = meshShaderBuilder.build_pipeline(engine->_device);

    vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
    vkDestroyShaderModule(engine->_device, meshMeshShader, nullptr);
    if (meshTaskShader != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(engine->_device, meshTaskShader, nullptr);
    }
}

MaterialInstance GLTFMetallic_Roughness::write_material(VkDevice device,
                                                        MaterialPass pass,
                                                        const MaterialResources& resources,
                                                        DescriptorAllocatorGrowable& descriptorAllocator)
{
    MaterialInstance matData;
    matData.passType = pass;
    if (pass == MaterialPass::Transparent)
    {
        matData.pipeline = &transparentPipeline;
    }
    else
    {
        matData.pipeline = &opaquePipeline;
    }

    matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

    writer.clear();
    writer.write_buffer(0,
                        resources.dataBuffer,
                        sizeof(MaterialConstants),
                        resources.dataBufferOffset,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write_image(1,
                       resources.colorImage.imageView,
                       resources.colorSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(2,
                       resources.metalRoughImage.imageView,
                       resources.metalRoughSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    writer.update_set(device, matData.materialSet);

    return matData;
}

void VulkanEngine::DrawGround(const glm::mat4& topMatrix)
{
    if (!_groundNode)
        return;
    _groundNode->Draw(topMatrix, mainDrawContext, 1);
}

void MeshNode::Draw(const glm::mat4& topMatrix, DrawContext& ctx, int InstanceCount)
{
    glm::mat4 nodeMatrix = topMatrix * worldTransform;

    for (auto& s : mesh->surfaces)
    {
        RenderObject def;
        def.indexCount = s.count;
        def.firstIndex = s.startIndex;
        def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
        def.material = &s.material->data;
        def.instanceCount = InstanceCount;

        def.transform = nodeMatrix;
        def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;
        def.instanceTransformBufferAddress = mesh->meshBuffers.instanceTransformBufferAddress;

        // Mesh-shader path: forward the (optional) meshlet buffer addresses.
        def.meshletBufferAddress = mesh->meshBuffers.meshletBufferAddress;
        def.meshletVerticesAddress = mesh->meshBuffers.meshletVerticesAddress;
        def.meshletTrianglesAddress = mesh->meshBuffers.meshletTrianglesAddress;
        def.meshletBoundsAddress = mesh->meshBuffers.meshletBoundsAddress;
        def.meshletCount = mesh->meshBuffers.meshletCount;

        ctx.OpaqueSurfaces.push_back(def);
    }

    // recurse down
    Node::Draw(topMatrix, ctx, InstanceCount);
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device)
{
    vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
    vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);

    vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
    vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);

    if (opaqueMeshPipeline.pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, opaqueMeshPipeline.pipeline, nullptr);
        vkDestroyPipelineLayout(device, opaqueMeshPipeline.layout, nullptr);
    }
}
