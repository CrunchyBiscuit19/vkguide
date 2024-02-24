// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_descriptors.h>
#include <vk_loader.h>
#include <vk_types.h>

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
    struct SDL_Window* _window { nullptr };
    VkExtent2D _windowExtent { 1700, 900 };

    bool _isInitialized { false };
    bool stop_rendering { false };

    VkInstance _instance; // Vulkan library handle
    VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle

    VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
    VkDevice _device; // Vulkan device for commands
    VkSurfaceKHR _surface; // Vulkan window surface

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    VmaAllocator _allocator;

    VkPipeline _gradientPipeline;
    VkPipelineLayout _gradientPipelineLayout;
    VkPipelineLayout _meshPipelineLayout;
    VkPipeline _meshPipeline;
    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundEffect { 0 };

    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    VkExtent2D _swapchainExtent;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    bool _resize_requested;

    AllocatedImage _whiteImage;
    AllocatedImage _blackImage;
    AllocatedImage _greyImage;
    AllocatedImage _errorCheckerboardImage;
    AllocatedImage _drawImage; // Drawn images before copying to swapchain
    AllocatedImage _depthImage;
    VkExtent2D _drawExtent;
    float _renderScale = 1.f;

    VkSampler _defaultSamplerLinear;
    VkSampler _defaultSamplerNearest;

    int _frameNumber { 0 };
    FrameData _frames[FRAME_OVERLAP];
    FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

    DescriptorAllocator _globalDescriptorAllocator;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;
    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
    VkDescriptorSetLayout _singleImageDescriptorLayout;

	GPUSceneData sceneData;

    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    DeletionQueue _mainDeletionQueue;

    std::vector<std::shared_ptr<MeshAsset>> testMeshes;

    static VulkanEngine& Get();
    void init(); // initializes everything in the engine
    void cleanup(); // shuts down the engine
    void draw(); // draw loop
    void run(); // run main loop

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) const;

    GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

private:
    void init_imgui();

    void init_vulkan();

    void create_swapchain(uint32_t width, uint32_t height);
    void init_swapchain();
    void destroy_swapchain() const;
    void resize_swapchain();

    void init_commands();

    void init_sync_structures();

    void init_descriptors();

    void init_pipelines();
    void init_background_pipelines();
    void init_mesh_pipeline();
    void init_default_data();

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void destroy_buffer(const AllocatedBuffer& buffer) const;

    AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage create_image(const void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    void destroy_image(const AllocatedImage& img) const;

    void draw_background(VkCommandBuffer cmd) const;
    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) const;
    void draw_geometry(VkCommandBuffer cmd);
};
