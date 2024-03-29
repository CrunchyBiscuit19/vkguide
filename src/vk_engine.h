// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <camera.h>
#include <vk_descriptors.h>
#include <vk_loader.h>
#include <vk_types.h>
#include <cvars.h>

constexpr unsigned int FRAME_OVERLAP = 2;

struct GLTFMetallic_Roughness {
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    VkDescriptorSetLayout materialLayout;

    struct MaterialConstants {
        glm::vec4 colorFactors;
        glm::vec4 metal_rough_factors;
        // Padding, we need it anyway for uniform buffers
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

    struct MaterialDeletionQueue {
        DeletionQueue<VkDescriptorSetLayout> descriptorSetLayoutDeletion;
        DeletionQueue<VkPipelineLayout> pipelineLayoutDeletion;
        DeletionQueue<VkPipeline> pipelineDeletion;
    } _materialDeletionQueue;

    void build_pipelines(VulkanEngine* engine);
    void cleanup_resources(VkDevice device);

    MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct MeshNode : public Node {
    std::shared_ptr<MeshAsset> mesh;

    void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct RenderObject {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    Bounds bounds;

    MaterialInstance* material;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
    std::vector<RenderObject> OpaqueSurfaces;
    std::vector<RenderObject> TransparentSurfaces;
};

struct EngineStats {
    float frametime;
    int triangle_count;
    int drawcall_count;
    float scene_update_time;
    float mesh_draw_time;
};

class VulkanEngine {
public:
    bool _isInitialized { false };
    bool _stopRendering { false };

    SDL_Window* _window { nullptr };
    VkExtent2D _windowExtent { 1700, 900 };

    VkInstance _instance; // Vulkan library handle
    VkDebugUtilsMessengerEXT _debugMessenger; // Vulkan debug output handle

    VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
    VkDevice _device; // Vulkan device for commands
    VkSurfaceKHR _surface; // Vulkan window surface

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    VmaAllocator _allocator;

    EngineStats stats;
    CVarSystem* cvarInstance { CVarSystem::Get() };

    VkPipeline _gradientPipeline;
    VkPipelineLayout _gradientPipelineLayout;
    VkPipelineLayout _meshPipelineLayout;
    VkPipeline _meshPipeline;
    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundEffect { 0 };
    struct PipelineDeletionQueue {
        DeletionQueue<VkPipelineLayout> pipelineLayouts;
        DeletionQueue<VkPipeline> pipelines;
    } _pipelineDeletionQueue;

    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    VkExtent2D _swapchainExtent;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    bool _resize_requested;
    struct SwapchainDeletionQueue {
        DeletionQueue<VkSwapchainKHR> swapchains;
        DeletionQueue<VkImageView> imageViews;
    } _swapchainDeletionQueue;

    DescriptorAllocatorGrowable _globalDescriptorAllocator;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;
    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
    VkDescriptorSetLayout _singleImageDescriptorLayout;
    struct DescriptorDeletionQueue {
        DeletionQueue<VkDescriptorSetLayout> descriptorSetLayouts;
    } _descriptorDeletionQueue;

    std::unordered_map<std::string, AllocatedImage> _stockImages;
    AllocatedImage _drawImage; // Drawn images before copying to swapchain
    AllocatedImage _depthImage;
    VkExtent2D _drawExtent;
    float _renderScale = 1.f;
    struct ImageDeletionQueue {
        DeletionQueue<VkImage> images;
        DeletionQueue<VkImageView> imageViews;
    } _imageDeletionQueue;

    VkSampler _defaultSamplerLinear;
    VkSampler _defaultSamplerNearest;
    struct SamplerDeletionQueue {
        DeletionQueue<VkSampler> samplers;
    } _samplerDeletionQueue;

    Camera mainCamera;

    int _frameNumber { 0 };
    FrameData _frames[FRAME_OVERLAP];
    FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; }

    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;
    VkDescriptorPool _imguiDescriptorPool;
    struct ImmediateDeletionQueue {
        DeletionQueue<VkFence> fences;
        DeletionQueue<VkCommandPool> commandPools;
    } _immediateDeletionQueue;

    struct BufferDeletionQueue {
        DeletionQueue<VkBuffer> buffers;
    } _genericBufferDeletionQueue;

    static VulkanEngine& Get();
    void init(); // initializes everything in the engine

    void cleanup_immediate();
    void cleanup_swapchain();
    void cleanup_descriptors();
    void cleanup_pipelines();
    void cleanup_samplers();
    void cleanup_images();
    void cleanup_buffers();
    void cleanup_imgui() const;
    void cleanup_misc() const;
    void cleanup_core() const;
    void cleanup(); // shuts down the engine

    void draw(); // draw loop
    void run(); // run main loop

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) const;

    GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    std::vector<std::shared_ptr<MeshAsset>> testMeshes;

    GPUSceneData sceneData;
    MaterialInstance defaultData;
    GLTFMetallic_Roughness metalRoughMaterial;

    DrawContext mainDrawContext;
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

    void init_imgui();

    void init_vulkan();

    void create_swapchain(uint32_t width, uint32_t height);
    void init_swapchain();
    void destroy_swapchain();
    void resize_swapchain();

    void init_commands();

    void init_sync_structures();

    void init_descriptors();

    void init_pipelines();
    void init_background_pipelines();
    void init_mesh_pipeline();
    void init_default_data();

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void destroy_buffer(const AllocatedBuffer& buffer);

    AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage create_image(const void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    void destroy_image(const AllocatedImage& img);

    void draw_background(VkCommandBuffer cmd) const;
    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) const;
    void draw_geometry(VkCommandBuffer cmd);

    void update_scene();
};
