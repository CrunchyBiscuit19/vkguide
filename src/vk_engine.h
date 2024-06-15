﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <camera.h>
#include <cvars.h>
#include <vk_descriptors.h>
#include <vk_loader.h>
#include <vk_materials.h>
#include <vk_types.h>

constexpr unsigned int FRAME_OVERLAP = 2;
constexpr unsigned int ONE_SECOND_IN_MILLISECONDS = 1000;
constexpr unsigned int EXPECTED_FRAME_RATE = 60;

struct EngineStats {
    float frametime;
    int triangle_count;
    int drawcall_count;
    float scene_update_time;
    float mesh_draw_time;
};

struct FrameData {
    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;

    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;

    DescriptorAllocatorGrowable _frameDescriptors;

    struct FrameDeletionQueue {
        DeletionQueue<VkFence> fenceDeletion;
        DeletionQueue<VkSemaphore> semaphoreDeletion;
        DeletionQueue<VkCommandPool> commandPoolDeletion;
        DeletionQueue<VkBuffer> bufferDeletion;
    } _frameDeletionQueue;

    void cleanup(VkDevice device)
    {
        _frameDeletionQueue.fenceDeletion.flush();
        _frameDeletionQueue.semaphoreDeletion.flush();
        _frameDeletionQueue.commandPoolDeletion.flush();
        _frameDeletionQueue.bufferDeletion.flush();
        _frameDescriptors.destroy_pools(device);
    }
};

class VulkanEngine {
public:
    // Engine state
    bool _isInitialized { false };
    bool _stopRendering { false };

    // Stats
    EngineStats stats;
    CVarSystem* cvarInstance { CVarSystem::Get() };

    // Window object
    SDL_Window* _window { nullptr };
    VkExtent2D _windowExtent { 1700, 900 };
    float _renderScale { 1.0f };

    // Vulkan stuff
    VkInstance _instance; // Vulkan library handle
    VkDebugUtilsMessengerEXT _debugMessenger; // Vulkan debug output handle

    VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
    VkDevice _device; // Vulkan device for commands
    VkSurfaceKHR _surface; // Vulkan window surface

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    // Frames
    int _frameNumber { 0 };
    FrameData _frames[FRAME_OVERLAP];
    FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; }
    FrameData& get_previous_frame() { return _frames[(_frameNumber - 1) % FRAME_OVERLAP]; }

    // VMA
    VmaAllocator _allocator;

    // Descriptor allocator
    DescriptorAllocatorGrowable _globalDescriptorAllocator;

    // Swapchain
    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    VkExtent2D _swapchainExtent;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    bool _resize_requested;

    // Pipeline things
    std::vector<char> pipelineCacheData;
    VkPipelineCache pipelineCache;
    std::unordered_map<std::size_t, MaterialPipeline> pipelinesCreated;

    // Images
    std::unordered_map<std::string, AllocatedImage> _stockImages;
    VkDescriptorSetLayout _stockImageDescriptorLayout;

    AllocatedImage _drawImage; // Drawn images before copying to swapchain
    VkDescriptorSetLayout _drawImageDescriptorLayout;
    VkDescriptorSet _drawImageDescriptors;
    VkExtent2D _drawExtent;

    AllocatedImage _depthImage;

    // Draw indirect-related
    AllocatedBuffer indirectVertexBuffer;
    AllocatedBuffer indirectIndexBuffer;
    // For each material, add to it the associated primitives
    std::unordered_map<PbrMaterial*, std::vector<VkDrawIndexedIndirectCommand>> indirectBatches;
    std::unordered_map<PbrMaterial*, AllocatedBuffer> indirectBuffers;
    AllocatedBuffer instanceBuffer;

    // Scene data
    SceneData sceneData;
    AllocatedBuffer sceneBuffer;

    // Models and materials
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedModels;
    AllocatedBuffer materialConstantsBuffer;
    VkDescriptorSetLayout materialTexturesArraySetLayout;
    VkDescriptorSet materialTexturesArrayDescriptorSet;

    // Samplers
    VkSampler _defaultSamplerLinear;
    VkSampler _defaultSamplerNearest;

    // Camera
    Camera mainCamera;

    // Immediate submit
    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;
    VkDescriptorPool _imguiDescriptorPool;

    // Deletion queues
    struct SwapchainDeletionQueue {
        DeletionQueue<VkSwapchainKHR> swapchains;
        DeletionQueue<VkImageView> imageViews;
    } _swapchainDeletionQueue;

    struct SamplerDeletionQueue {
        DeletionQueue<VkSampler> samplers;
    } _samplerDeletionQueue;

    struct ImmediateDeletionQueue {
        DeletionQueue<VkFence> fences;
        DeletionQueue<VkCommandPool> commandPools;
    } _immediateDeletionQueue;

    struct PipelineDeletionQueue {
        DeletionQueue<VkPipeline> pipelines;
        DeletionQueue<VkPipelineLayout> pipelineLayouts;
    } _pipelineDeletionQueue;

    struct BufferDeletionQueue {
        DeletionQueue<VkBuffer> genericBuffers;
        DeletionQueue<VkBuffer> perDrawBuffers;
        DeletionQueue<VkBuffer> tempBuffers;
    } _bufferDeletionQueue;

    struct DescriptorDeletionQueue {
        DeletionQueue<VkDescriptorSetLayout> descriptorSetLayouts;
    } _descriptorDeletionQueue;

    struct ImageDeletionQueue {
        DeletionQueue<VkImage> images;
        DeletionQueue<VkImageView> imageViews;
    } _imageDeletionQueue;

    static VulkanEngine& Get();
    void init(); // initializes everything in the engine
    void init_imgui();
    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
    void init_descriptors();
    void init_pipeline_caches();
    void init_pipelines();
    void init_default_data();
    void init_models(const std::vector<std::string>& modelPaths);

    void create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();
    void resize_swapchain();

    VkPipelineCacheCreateInfo read_pipeline_cache(const std::string& filename);
    void write_pipeline_cache(const std::string& filename);
    MaterialPipeline create_pipeline(bool doubleSided, fastgltf::AlphaMode alphaMode);

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, DeletionQueue<VkBuffer>& bufferDeletionQueue);
    void destroy_buffer(const AllocatedBuffer& buffer, DeletionQueue<VkBuffer>& bufferDeletionQueue);

    AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage create_image(const void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    void destroy_image(const AllocatedImage& img);

    MeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) const;
    void draw_geometry(VkCommandBuffer cmd);

    void create_vertex_index_buffers();
    void update_vertex_index_buffers(AllocatedBuffer srcVertex, AllocatedBuffer dstVertex, int vertexOffset,
    AllocatedBuffer srcIndex, AllocatedBuffer dstIndex, int indexOffset);
    void update_indirect_commands(Primitive& primitive, int& verticesOffset, int& indicesOffset, int& primitivesOffset);
    void iterate_primitives();
    void update_indirect_batches();
    void update_instanced_data();
    void update_scene_buffer();
    void update_material_buffer(PbrMaterial& material);
    void update_material_texture_array(PbrMaterial& material);
    void update_scene();

    void cleanup_immediate();
    void cleanup_swapchain();
    void cleanup_descriptors();
    void cleanup_pipeline_caches();
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
