// vulkan_guide.h : Include file for standard system include files,
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
    VkCommandPool mCommandPool;
    VkCommandBuffer mMainCommandBuffer;

    VkSemaphore mSwapchainSemaphore, mRenderSemaphore;
    VkFence mRenderFence;

    DescriptorAllocatorGrowable mFrameDescriptors;

    struct FrameDeletionQueue {
        DeletionQueue<VkFence> fenceDeletion;
        DeletionQueue<VkSemaphore> semaphoreDeletion;
        DeletionQueue<VkCommandPool> commandPoolDeletion;
        DeletionQueue<VkBuffer> bufferDeletion;
    } mFrameDeletionQueue;

    void cleanup(VkDevice device)
    {
        mFrameDeletionQueue.fenceDeletion.flush();
        mFrameDeletionQueue.semaphoreDeletion.flush();
        mFrameDeletionQueue.commandPoolDeletion.flush();
        mFrameDeletionQueue.bufferDeletion.flush();
        mFrameDescriptors.destroy_pools(device);
    }
};

class VulkanEngine {
public:
    // Engine state
    bool mIsInitialized { false };
    bool mStopRendering { false };

    // Stats
    EngineStats mStats;

    // Window object
    SDL_Window* mWindow { nullptr };
    VkExtent2D mWindowExtent { 1700, 900 };
    float mRenderScale { 1.0f };

    // Vulkan stuff
    VkInstance mInstance; // Vulkan library handle
    VkDebugUtilsMessengerEXT mDebugMessenger; // Vulkan debug output handle

    VkPhysicalDevice mChosenGPU; // GPU chosen as the default device
    VkDevice mDevice; // Vulkan device for commands
    VkSurfaceKHR mSurface; // Vulkan window surface

    VkQueue mGraphicsQueue;
    uint32_t mGraphicsQueueFamily;

    // Frames
    int mFrameNumber { 0 };
    FrameData mFrames[FRAME_OVERLAP];
    FrameData& get_current_frame() { return mFrames[mFrameNumber % FRAME_OVERLAP]; }
    FrameData& get_previous_frame() { return mFrames[(mFrameNumber - 1) % FRAME_OVERLAP]; }

    // VMA
    VmaAllocator mAllocator;

    // Descriptor allocator
    DescriptorAllocatorGrowable mGlobalDescriptorAllocator;

    // Swapchain
    VkSwapchainKHR mSwapchain;
    VkFormat mSwapchainImageFormat;
    VkExtent2D mSwapchainExtent;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    bool mResizeRequested;

    // Pipeline things
    std::vector<char> mPipelineCacheData;
    VkPipelineCache mPipelineCache;
    std::unordered_map<std::size_t, MaterialPipeline> mPipelinesCreated;

    // Images
    std::unordered_map<std::string, AllocatedImage> mStockImages;
    VkDescriptorSetLayout mStockImageDescriptorLayout;

    AllocatedImage mDrawImage; // Drawn images before copying to swapchain
    VkDescriptorSetLayout mDrawImageDescriptorLayout;
    VkDescriptorSet mDrawImageDescriptors;
    VkExtent2D mDrawExtent;

    AllocatedImage mDepthImage;

    // Draw indirect-related
    AllocatedBuffer mIndirectVertexBuffer;
    AllocatedBuffer mIndirectIndexBuffer;
    // For each material, add to it the associated primitives
    std::unordered_map<PbrMaterial*, std::vector<VkDrawIndexedIndirectCommand>> mIndirectBatches;
    std::unordered_map<PbrMaterial*, AllocatedBuffer> mIndirectBuffers;
    AllocatedBuffer mInstanceBuffer;

    // Scene data
    SceneData mSceneData;
    AllocatedBuffer mSceneBuffer;

    // Models and materials
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> mLoadedModels;
    AllocatedBuffer mMaterialConstantsBuffer;
    VkDescriptorSetLayout mMaterialTexturesArraySetLayout;
    VkDescriptorSet mMaterialTexturesArrayDescriptorSet;

    // Samplers
    VkSampler mDefaultSamplerLinear;
    VkSampler mDefaultSamplerNearest;

    // Camera
    Camera mMainCamera;

    // Immediate submit
    VkFence mImmFence;
    VkCommandBuffer mImmCommandBuffer;
    VkCommandPool mImmCommandPool;
    VkDescriptorPool mImguiDescriptorPool;

    // Deletion queues
    struct SwapchainDeletionQueue {
        DeletionQueue<VkSwapchainKHR> swapchains;
        DeletionQueue<VkImageView> imageViews;
    } mSwapchainDeletionQueue;

    struct SamplerDeletionQueue {
        DeletionQueue<VkSampler> samplers;
    } mSamplerDeletionQueue;

    struct ImmediateDeletionQueue {
        DeletionQueue<VkFence> fences;
        DeletionQueue<VkCommandPool> commandPools;
    } mImmediateDeletionQueue;

    struct PipelineDeletionQueue {
        DeletionQueue<VkPipeline> pipelines;
        DeletionQueue<VkPipelineLayout> pipelineLayouts;
    } mPipelineDeletionQueue;

    struct BufferDeletionQueue {
        DeletionQueue<VkBuffer> genericBuffers;
        DeletionQueue<VkBuffer> perDrawBuffers;
        DeletionQueue<VkBuffer> tempBuffers;
    } mBufferDeletionQueue;

    struct DescriptorDeletionQueue {
        DeletionQueue<VkDescriptorSetLayout> descriptorSetLayouts;
    } mDescriptorDeletionQueue;

    struct ImageDeletionQueue {
        DeletionQueue<VkImage> images;
        DeletionQueue<VkImageView> imageViews;
    } mImageDeletionQueue;

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
