// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <camera.h>
#include <cvars.h>
#include <vk_descriptors.h>
#include <vk_materials.h>
#include <vk_models.h>
#include <vk_types.h>

#include <map>

constexpr unsigned int FRAME_OVERLAP = 2;
constexpr unsigned int ONE_SECOND_IN_MILLISECONDS = 1000;
constexpr unsigned int EXPECTED_FRAME_RATE = 60;

constexpr unsigned int ONE_MEBIBYTE_IN_BYTES = 1048576;

constexpr unsigned int MAX_IMAGE_SIZE = 100 * ONE_MEBIBYTE_IN_BYTES;

constexpr unsigned int DEFAULT_VERTEX_BUFFER_SIZE = 20 * ONE_MEBIBYTE_IN_BYTES;
constexpr unsigned int DEFAULT_INDEX_BUFFER_SIZE = 20 * ONE_MEBIBYTE_IN_BYTES;

constexpr unsigned int MAX_INSTANCES = 5000;

constexpr unsigned int MAX_INDIRECT_COMMANDS = 10000;

constexpr unsigned int MAX_MATERIALS = 5000;

constexpr unsigned int MAX_TRANSFORM_MATRICES = 5000;

constexpr unsigned int OBJECT_COUNT = 1;

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

    // Pipelines
    std::vector<char> mPipelineCacheData;
    VkPipelineCache mPipelineCache;
    std::unordered_map<std::size_t, MaterialPipeline> mPipelinesCreated;

    // Push constants
    SSBOAddresses mPushConstants;

    // Images
    std::unordered_map<std::string, AllocatedImage> mStockImages;

    AllocatedImage mDrawImage; // Drawn images before copying to swapchain
    DescriptorCombined mDrawImageDescriptor;
    VkExtent2D mDrawExtent;

    AllocatedImage mDepthImage;

    // Store copy instructions for buffers
    struct BufferCopyBatches {
        std::vector<BufferCopyBatch> perDrawBuffers;
        std::vector<BufferCopyBatch> modelBuffers;
    } mBufferCopyBatches;

    // Geometry data
    AllocatedBuffer mGlobalVertexBuffer;
    AllocatedBuffer mGlobalIndexBuffer;

    // Instance data
    AllocatedBuffer mInstanceBuffer;

    // Scene data
    SceneData mSceneData;
    AllocatedBuffer mSceneBuffer;

    // Models and materials
    std::unordered_map<std::string, std::shared_ptr<GLTFModel>> mLoadedModels;
    std::vector<glm::mat4> mMeshTransformMatrices;
    AllocatedBuffer mMeshTransformsBuffer;
    AllocatedBuffer mMaterialConstantsBuffer;
    DescriptorCombined mMaterialTexturesArray;

    // Store indirect commands per material
    std::map<IndirectBatchGroup, IndirectBatchData> mIndirectBatches;
    AllocatedBuffer mGlobalIndirectBuffer;
    std::unordered_map<MeshData*,int> mMeshIndexes; // These 2 unordered maps exist mostly to keep track of which meshes and materials have already been processed
    std::unordered_map<PbrMaterial*,int> mMatIndexes;

    // Samplers
    VkSampler mDefaultSamplerLinear;
    VkSampler mDefaultSamplerNearest;

    // Camera
    Camera mMainCamera;

    // Immediate submit
    struct immSubmit {
        VkFence fence;
        VkCommandBuffer commandBuffer;
        VkCommandPool commandPool;
        VkDescriptorPool imguiDescriptorPool;
    } mImmSubmit;

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
        DeletionQueue<VkBuffer> lifetimeBuffers;
        DeletionQueue<VkBuffer> perDrawBuffers;
        DeletionQueue<VkBuffer> modelLoadStagingBuffers;
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
    void init_buffers();
    void init_default_data();
    void init_models(const std::vector<std::filesystem::path>& modelFilePaths);
    void init_push_constants();

    void create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();
    void resize_swapchain();

    VkPipelineCacheCreateInfo read_pipeline_cache(const std::string& filename);
    void write_pipeline_cache(const std::string& filename);
    MaterialPipeline create_pipeline(bool doubleSided, fastgltf::AlphaMode alphaMode);

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, DeletionQueue<VkBuffer>& bufferDeletionQueue);
    void destroy_buffer(const AllocatedBuffer& buffer, DeletionQueue<VkBuffer>& bufferDeletionQueue) const;

    AllocatedImage create_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage create_image(const void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    void destroy_image(const AllocatedImage& img);

    ModelBuffers upload_model(std::vector<uint32_t>& indices, std::vector<Vertex>& vertices);

    AllocatedBuffer create_staging_buffer(size_t allocSize, DeletionQueue<VkBuffer>& bufferDeletionQueue);
    void create_vertex_index_buffers();
    void create_instance_buffer();
    void create_scene_buffer();
    void create_mesh_transform_buffer();
    void create_material_constants_buffer();
    void create_indirect_buffer();

    void update_vertex_index_buffers(AllocatedBuffer srcVertexBuffer, int& vertexBufferOffset, AllocatedBuffer srcIndexBuffer, int& indexBufferOffset);
    void generate_indirect_commands(MeshData& mesh, Primitive& primitive, int& verticesOffset, int& indicesOffset);
    void assign_indirect_groups(MeshData& mesh, Primitive& primitive);
    void traverse_nodes(Node& startingNode, std::vector<glm::mat4>& meshWorldTransformMatrices, int& meshIndex);
    void iterate_models();

    void update_indirect_buffer();
    void update_instanced_buffer();
    void update_scene_buffer();
    void update_mesh_transform_buffer();
    void update_material_buffer();
    void update_material_texture_array();
    void submit_buffer_updates(std::vector<BufferCopyBatch>& bufferCopyBatches) const;
    void update_draw_data();

    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) const;
    void draw_geometry(VkCommandBuffer cmd);
    void draw(); // draw loop

    void run(); // run main loop

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) const;

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
    void cleanup_per_draw();
    void cleanup_core() const;
    void cleanup(); // shuts down the engine
};
