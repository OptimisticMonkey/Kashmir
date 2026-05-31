// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <camera.h>
#include <vk_loader.h>
#include <vk_descriptors.h>

struct GLTFMetallic_Roughness {
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;
	// Optional mesh-shader variant of opaquePipeline. Bound in draw_geometry
	// when VulkanEngine::_useMeshShaders is true. Has its own layout because
	// the push-constant range differs (GPUMeshShaderPushConstants vs GPUDrawPushConstants).
	MaterialPipeline opaqueMeshPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	void build_pipelines(VulkanEngine* engine);
	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	uint32_t instanceCount;

	MaterialInstance* material;

	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
	VkDeviceAddress instanceTransformBufferAddress;

	// Mesh-shader path. Zero when the mesh wasn't clustered (e.g. ground).
	VkDeviceAddress meshletBufferAddress{ 0 };
	VkDeviceAddress meshletVerticesAddress{ 0 };
	VkDeviceAddress meshletTrianglesAddress{ 0 };
	VkDeviceAddress meshletBoundsAddress{ 0 };
	uint32_t        meshletCount{ 0 };
};


struct GPUSceneData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
	glm::mat4 lightViewProj;      // sun's view-projection (for shadow mapping)
	glm::vec4 ambientColor;       // sky color (hemispheric ambient: surfaces facing up)
	glm::vec4 sunlightDirection;  // w for sun power
	glm::vec4 sunlightColor;
	glm::vec4 cameraPos;          // world-space camera position
	glm::vec4 groundColor;        // hemispheric ambient: surfaces facing down
	// .x = shadowMode (0 = rasterized shadow map, 1 = raytraced ray-query).
	// Other components reserved. Kept as a float4 to preserve std140 alignment
	// for the UBO. Mirrored in shaders/common.slang::SceneData.shadowParams.
	glm::vec4 shadowParams;
	// Mesh-shader debug-view flags. .x = cluster color, .y = lit cluster color.
	// Lives in the UBO (not push constants) so the regular non-mesh-shader
	// fragment pipeline — whose push range is VERTEX-only and 80 bytes — can read
	// it without out-of-range / wrong-stage push-constant access.
	glm::vec4 debugParams;
};

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct UpdateTransformPushConstants {
	VkDeviceAddress instanceTransformBuffer;
	float time;
	uint32_t count;
	float padX;
	float padY;
};

struct ComputeEffect {
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
};

struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx, int InstanceCount) override;
};

struct FrameData {

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;

	DeletionQueue _deletionQueue;
	DescriptorAllocatorGrowable _frameDescriptors;
};

constexpr unsigned int FRAME_OVERLAP = 2;


class VulkanEngine {
public:

	Camera mainCamera;

	DrawContext mainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

	void update_scene();

	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;

	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false, const char* debugName = nullptr);
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false, const char* debugName = nullptr);
	void destroy_image(const AllocatedImage& img);

	GPUSceneData sceneData;

	

	// immediate submit structures
	std::vector<VkSemaphore> _presentSemaphores;  // one binary semaphore per swapchain image
	void rebuild_present_semaphores();
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);


	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	//DescriptorAllocator globalDescriptorAllocator;
	DescriptorAllocatorGrowable globalDescriptorAllocator;

	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;
	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
	VkDescriptorSetLayout _singleImageDescriptorLayout;

	VmaAllocator _allocator;
	//draw resources
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	VkExtent2D _drawExtent;

	// Shadow map (single directional sun, single cascade).
	AllocatedImage _shadowImage;
	VkSampler _shadowSampler{ VK_NULL_HANDLE };
	VkExtent2D _shadowExtent{ 2048, 2048 };
	VkPipeline _shadowPipeline{ VK_NULL_HANDLE };
	VkPipelineLayout _shadowPipelineLayout{ VK_NULL_HANDLE };
	// Mesh-shader variant — bound when _useMeshShaders is true.
	VkPipeline _shadowMeshPipeline{ VK_NULL_HANDLE };
	VkPipelineLayout _shadowMeshPipelineLayout{ VK_NULL_HANDLE };


	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	bool resize_requested{ false };
	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };
	struct SDL_Gamepad* _controller{ nullptr };
	glm::vec2 _padLeftAxis{ 0.f };

	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();
	void draw_geometry(VkCommandBuffer cmd);
	void draw_background(VkCommandBuffer cmd);
	void draw_shadow(VkCommandBuffer cmd);
	void update_transform(VkCommandBuffer cmd);
	void DrawGround(const glm::mat4& topMatrix);
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void draw_stats_overlay();
	//run main loop
	void run();

	VkInstance _instance;// Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger;// Vulkan debug output handle
	VkPhysicalDevice _chosenGPU;// GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface;// Vulkan window surface

	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;

	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	FrameData _frames[FRAME_OVERLAP];

	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
	// Builds meshlet clusters with meshoptimizer and uploads the GPU buffers
	// (meshlets, vertex indices, packed triangle indices) into mesh.meshBuffers.
	void InitClusters(MeshAsset& mesh, std::span<Vertex> vertices, std::span<uint32_t> indices);

	// Runtime toggle: when true, draw_geometry routes Suzanne's opaque surfaces
	// through opaqueMeshPipeline + vkCmdDrawMeshTasksEXT instead of vkCmdDrawIndexed.
	bool _useMeshShaders{ false };
	// Debug view: paint each meshlet a hashed color. Only active on the mesh-shader
	// path. _debugClusterLit gates whether PBR lighting is applied on top (otherwise
	// the fragment shader outputs the flat cluster color directly).
	bool _debugClusterColor{ false };
	bool _debugClusterLit{ false };
	// Loaded from vkGetDeviceProcAddr in init_vulkan.
	PFN_vkCmdDrawMeshTasksEXT pfnCmdDrawMeshTasksEXT{ nullptr };

	// --- Raytraced shadows (VK_KHR_ray_query) ---------------------------------
	// Runtime toggle: when true, draw() skips draw_shadow() and instead builds
	// a TLAS via build_tlas(); the fragment shaders read sceneTLAS for occlusion.
	bool _useRaytracedShadows{ false };

	// Loaded from vkGetDeviceProcAddr in init_vulkan.
	PFN_vkCreateAccelerationStructureKHR        pfnCreateAccelerationStructureKHR{ nullptr };
	PFN_vkDestroyAccelerationStructureKHR       pfnDestroyAccelerationStructureKHR{ nullptr };
	PFN_vkCmdBuildAccelerationStructuresKHR     pfnCmdBuildAccelerationStructuresKHR{ nullptr };
	PFN_vkGetAccelerationStructureBuildSizesKHR pfnGetAccelerationStructureBuildSizesKHR{ nullptr };
	PFN_vkGetAccelerationStructureDeviceAddressKHR pfnGetAccelerationStructureDeviceAddressKHR{ nullptr };

	VkDeviceSize _asScratchAlignment{ 256 };
	AllocatedBuffer _asScratchBuffer{};

	// Placeholder TLAS bound when raytraced shadows are disabled so the
	// descriptor at binding 2 always references a valid acceleration structure.
	VkAccelerationStructureKHR _placeholderTlas{ VK_NULL_HANDLE };
	AllocatedBuffer            _placeholderTlasBuffer{};

	// Live TLAS rebuilt each frame when raytraced shadows are enabled. The
	// previous frame's handle + storage buffer are pushed into the frame
	// deletion queue when a new one is built.
	VkAccelerationStructureKHR _tlas{ VK_NULL_HANDLE };

	// Compute pipeline that writes VkAccelerationStructureInstanceKHR records
	// from animated instance transforms.
	VkPipeline       _tlasInstancePipeline{ VK_NULL_HANDLE };
	VkPipelineLayout _tlasInstancePipelineLayout{ VK_NULL_HANDLE };

	void init_raytracing();
	void init_tlas_instance_pipeline();
	void build_blas(struct MeshAsset& mesh);
	void destroy_blas(struct GPUMeshBuffers& mb);
	void build_tlas(VkCommandBuffer cmd);

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, const char* debugName);
	void destroy_buffer(const AllocatedBuffer& buffer);


private:

	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();
	void resize_swapchain();

	DeletionQueue _mainDeletionQueue;
	void init_descriptors();

	void init_pipelines();
	void init_background_pipelines();
	void init_update_transform_pipeline();
	void init_ground_pipeline();
	void init_shadow_resources();
	void init_shadow_pipeline();
	VkPipeline _updateTransformPipeline;
	VkPipelineLayout _updateTransformPipelineLayout;
	MaterialPipeline _groundPipeline;   // layout shared with metalRoughMaterial
	MaterialInstance _groundMaterial;
	void init_imgui();
	VkPipelineLayout _trianglePipelineLayout;
	VkPipeline _trianglePipeline;

	void init_triangle_pipeline();


	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;

	GPUMeshBuffers rectangle;

	void init_mesh_pipeline();
	void init_default_data();

	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	std::shared_ptr<MeshNode> _groundNode;

	uint32_t _monkeyInstanceCount{ 0 };
	double _lastMonkeySpawnTime{ 0.0 };

	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
};
