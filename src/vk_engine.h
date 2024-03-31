// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <camera.h>
#include <cvars.h>
#include <vk_descriptors.h>
#include <vk_loader.h>
#include <vk_types.h>
#include <vk_materials.h>

constexpr unsigned int FRAME_OVERLAP = 2;

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

    VkPipelineLayout _meshPipelineLayout;
    VkPipeline _meshPipeline;
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

    GPUSceneData sceneData;
    GLTFMetallicRough metalRoughMaterial;

    DrawContext mainDrawContext; // List of things to draw
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

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
    void init_imgui();
    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
    void init_descriptors();
    void init_pipelines();
    void init_mesh_pipeline();
    void init_default_data();

    void create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();
    void resize_swapchain();

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void destroy_buffer(const AllocatedBuffer& buffer);

    AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage create_image(const void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    void destroy_image(const AllocatedImage& img);

    GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) const;
    void draw_geometry(VkCommandBuffer cmd);

    void update_scene();

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
};
