//> includes
#include <vk_engine.h>
#include <vk_images.h>
#include <vk_initializers.h>
#include <vk_pipelines.h>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#define VMA_IMPLEMENTATION
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <magic_enum.hpp>
#include <vk_mem_alloc.h>

#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <thread>

#ifdef NDEBUG
constexpr bool bUseValidationLayers = false;
#else
constexpr bool bUseValidationLayers = true;
#endif

const std::string pipelineCacheFile = "../../bin/pipeline_cache.bin";
const std::vector<std::string> modelFilepaths {
    //    "../../assets/scifihelmet/SciFiHelmet.glb",
    //    "../../assets/stainedglasslamp/StainedGlassLamp.gltf",
    //    "../../assets/stainedglasslamp/StainedGlassLamp4Meshes.gltf",
    //    "../../assets/AntiqueCamera/AntiqueCamera.glb",
    //    "../../assets/AntiqueCamera/AntiqueCameraSingleMesh.gltf"
    //    "../../assets/toycar/toycar.glb",
    //    "../../assets/sponza/Sponza.gltf",
    //    "../../assets/structure/structure.glb",
};

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get()
{
    return *loadedEngine;
}

void VulkanEngine::init()
{
    // Only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    mWindow = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        static_cast<int>(mWindowExtent.width),
        static_cast<int>(mWindowExtent.height),
        window_flags);

    init_vulkan();
    init_swapchain();
    init_commands();
    init_sync_structures();
    init_descriptors();
    init_pipelines();
    init_buffers();
    init_imgui();
    init_default_data();
    init_models(modelFilepaths);
    init_push_constants();
    mMainCamera.init();

    // Everything went fine
    mIsInitialized = true;
}

void VulkanEngine::init_imgui()
{
    // Create descriptor pool for IMGUI copied from demo
    constexpr VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(mDevice, &pool_info, nullptr, &mImmSubmit.imguiDescriptorPool));

    // Initialize imgui library
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(mWindow);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = mInstance;
    init_info.PhysicalDevice = mChosenGPU;
    init_info.Device = mDevice;
    init_info.Queue = mGraphicsQueue;
    init_info.DescriptorPool = mImmSubmit.imguiDescriptorPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.ColorAttachmentFormat = mSwapchainImageFormat;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

    immediate_submit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void VulkanEngine::init_vulkan()
{
    vkb::InstanceBuilder builder;

    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                        .request_validation_layers(bUseValidationLayers)
                        .use_default_debug_messenger()
                        .require_api_version(1, 3, 0)
                        .build();

    const vkb::Instance vkb_inst = inst_ret.value();

    mInstance = vkb_inst.instance;
    mDebugMessenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(mWindow, mInstance, &mSurface);

    VkPhysicalDeviceVulkan13Features features13 {};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;
    VkPhysicalDeviceVulkan12Features features12 {};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    features12.drawIndirectCount = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.runtimeDescriptorArray = true;
    features12.descriptorBindingSampledImageUpdateAfterBind = true;
    features12.descriptorBindingVariableDescriptorCount = true;
    VkPhysicalDeviceFeatures features {};
    features.multiDrawIndirect = true;

    // Use vkbootstrap to select a gpu.
    // A gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector { vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector
                                             .set_minimum_version(1, 3)
                                             .set_required_features_13(features13)
                                             .set_required_features_12(features12)
                                             .set_required_features(features)
                                             .set_surface(mSurface)
                                             .select()
                                             .value();
    vkb::DeviceBuilder deviceBuilder { physicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    mDevice = vkbDevice.device;
    mChosenGPU = physicalDevice.physical_device;

    // Get a graphics queue / family
    mGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // Initialize the memory _allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = mChosenGPU;
    allocatorInfo.device = mDevice;
    allocatorInfo.instance = mInstance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &mAllocator);
}

void VulkanEngine::init_swapchain()
{
    create_swapchain(mWindowExtent.width, mWindowExtent.height);

    // draw image size will match the window
    const VkExtent3D drawImageExtent = {
        mWindowExtent.width,
        mWindowExtent.height,
        1
    };

    // Hardcoding the draw format to 16 bit floats, extra precision for lighting calculations and better rendering.
    mDrawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    mDrawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages {};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const VkImageCreateInfo rimg_info = vkinit::image_create_info(mDrawImage.imageFormat, drawImageUsages, drawImageExtent);

    // Allocate draw image from GPU local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(mAllocator, &rimg_info, &rimg_allocinfo, &mDrawImage.image, &mDrawImage.allocation, nullptr);

    // Build a image-view for the draw image to use for rendering
    const VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(mDrawImage.imageFormat, mDrawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(mDevice, &rview_info, nullptr, &mDrawImage.imageView));

    mDepthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    mDepthImage.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages {};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    const VkImageCreateInfo dimg_info = vkinit::image_create_info(mDepthImage.imageFormat, depthImageUsages, drawImageExtent);
    vmaCreateImage(mAllocator, &dimg_info, &rimg_allocinfo, &mDepthImage.image, &mDepthImage.allocation, nullptr);

    const VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(mDepthImage.imageFormat, mDepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(mDevice, &dview_info, nullptr, &mDepthImage.imageView));

    destroy_image(mDrawImage);
    destroy_image(mDepthImage);
}

void VulkanEngine::init_commands()
{
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    const VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(mGraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // One command pool and command buffer per frame stored
    for (FrameData& frame : mFrames) {
        VK_CHECK(vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &frame.mCommandPool));
        // Allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(frame.mCommandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &frame.mMainCommandBuffer));

        frame.mFrameDeletionQueue.commandPoolDeletion.push_resource(mDevice, frame.mCommandPool, nullptr);
    }

    // Immediate submits
    VK_CHECK(vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &mImmSubmit.commandPool));
    const VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(mImmSubmit.commandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &mImmSubmit.commandBuffer));

    mImmediateDeletionQueue.commandPools.push_resource(mDevice, mImmSubmit.commandPool, nullptr);
}

void VulkanEngine::init_sync_structures()
{
    // 1 fence to control when the gpu has finished rendering the frame,
    // 2 semaphores to syncronize rendering with swapchain
    // Fence to start signalled so we can wait on it on the first frame
    const VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (FrameData& frame : mFrames) {
        VK_CHECK(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &frame.mRenderFence));
        VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &frame.mSwapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &frame.mRenderSemaphore));

        frame.mFrameDeletionQueue.fenceDeletion.push_resource(mDevice, frame.mRenderFence, nullptr);
        frame.mFrameDeletionQueue.semaphoreDeletion.push_resource(mDevice, frame.mSwapchainSemaphore, nullptr);
        frame.mFrameDeletionQueue.semaphoreDeletion.push_resource(mDevice, frame.mRenderSemaphore, nullptr);
    }

    // Immediate fence
    VK_CHECK(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &mImmSubmit.fence));
    mImmediateDeletionQueue.fences.push_resource(mDevice, mImmSubmit.fence, nullptr);
}

void VulkanEngine::init_descriptors()
{
    // Create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10000 },
    };
    mGlobalDescriptorAllocator.init(mDevice, 10, sizes);

    // Create descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        mDrawImageDescriptor.layout = builder.build(mDevice, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    // Allocate a descriptor set for our draw image
    mDrawImageDescriptor.set = mGlobalDescriptorAllocator.allocate(mDevice, mDrawImageDescriptor.layout);
    DescriptorWriter writer;
    writer.write_image(0, mDrawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(mDevice, mDrawImageDescriptor.set);

    // Create descriptor set layout for texture array
    int materialTexturesArraySize = 1000;
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, materialTexturesArraySize);
        mMaterialTexturesArray.layout = builder.build(mDevice, VK_SHADER_STAGE_FRAGMENT_BIT, true);
    }
    // Allocate a descriptor set for texture array
    mMaterialTexturesArray.set = mGlobalDescriptorAllocator.allocate(mDevice, mMaterialTexturesArray.layout, true, materialTexturesArraySize);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        mFrames[i].mFrameDescriptors = DescriptorAllocatorGrowable {};
        mFrames[i].mFrameDescriptors.init(mDevice, 1000, frame_sizes);
    }

    mDescriptorDeletionQueue.descriptorSetLayouts.push_resource(mDevice, mDrawImageDescriptor.layout, nullptr);
    mDescriptorDeletionQueue.descriptorSetLayouts.push_resource(mDevice, mMaterialTexturesArray.layout, nullptr);
}

void VulkanEngine::init_pipeline_caches()
{
    const VkPipelineCacheCreateInfo pipelineCacheCreateInfo = read_pipeline_cache(pipelineCacheFile);
    VK_CHECK(vkCreatePipelineCache(mDevice, &pipelineCacheCreateInfo, nullptr, &mPipelineCache));
}

void VulkanEngine::init_pipelines()
{
}

void VulkanEngine::init_buffers()
{
    create_vertex_index_buffers();
    create_instance_buffer();
    create_scene_buffer();
    create_material_constants_buffer();
    create_indirect_buffer();
}

void VulkanEngine::init_default_data()
{
    // Colour data interpreted as little endian
    constexpr uint32_t white = std::byteswap(0xFFFFFFFF);
    mStockImages["white"] = create_image(&white, VkExtent3D { 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    constexpr uint32_t grey = std::byteswap(0xAAAAAAFF);
    mStockImages["grey"] = create_image(&grey, VkExtent3D { 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    constexpr uint32_t black = std::byteswap(0x000000FF);
    mStockImages["black"] = create_image(&black, VkExtent3D { 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    constexpr uint32_t blue = std::byteswap(0x769DDBFF);
    mStockImages["blue"] = create_image(&blue, VkExtent3D { 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    // 16x16 checkerboard texture
    std::array<uint32_t, 16 * 16> pixels;
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            // constexpr uint32_t magenta = 0xFF00FFFF;
            constexpr uint32_t magenta = std::byteswap(0xFF00FFFF);
            pixels[static_cast<std::array<uint32_t, 256Ui64>::size_type>(y) * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    mStockImages["errorCheckerboard"] = create_image(pixels.data(), VkExtent3D { 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    // Default samplers
    VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(mDevice, &sampl, nullptr, &mDefaultSamplerNearest);
    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(mDevice, &sampl, nullptr, &mDefaultSamplerLinear);

    mSamplerDeletionQueue.samplers.push_resource(mDevice, mDefaultSamplerLinear, nullptr);
    mSamplerDeletionQueue.samplers.push_resource(mDevice, mDefaultSamplerNearest, nullptr);
}

void VulkanEngine::init_models(const std::vector<std::string>& modelPaths)
{
    for (const auto& modelPath : modelPaths) {
        const auto modelFile = load_gltf(this, modelPath);
        assert(modelFile.has_value());
        mLoadedModels[std::filesystem::path(modelPath).filename().string()] = *modelFile;
    }
}

void VulkanEngine::init_push_constants()
{
    VkBufferDeviceAddressInfo deviceAddressInfo { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = mGlobalVertexBuffer.buffer };
    mPushConstants.vertexBuffer = vkGetBufferDeviceAddress(mDevice, &deviceAddressInfo);
    deviceAddressInfo.buffer = mInstanceBuffer.buffer;
    mPushConstants.instanceBuffer = vkGetBufferDeviceAddress(mDevice, &deviceAddressInfo);
    deviceAddressInfo.buffer = mSceneBuffer.buffer;
    mPushConstants.sceneBuffer = vkGetBufferDeviceAddress(mDevice, &deviceAddressInfo);
    deviceAddressInfo.buffer = mMaterialConstantsBuffer.buffer;
    mPushConstants.materialsBuffer = vkGetBufferDeviceAddress(mDevice, &deviceAddressInfo);
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder { mChosenGPU, mDevice, mSurface };

    mSwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM; // Why is this BGRA?
    vkb::Swapchain vkbSwapchain = swapchainBuilder
                                      //.use_default_format_selection()
                                      .set_desired_format(VkSurfaceFormatKHR { .format = mSwapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
                                      // use vsync present mode
                                      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                      .set_desired_extent(width, height)
                                      .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                      .build()
                                      .value();

    mSwapchainExtent = vkbSwapchain.extent;
    mSwapchain = vkbSwapchain.swapchain;
    mSwapchainImages = vkbSwapchain.get_images().value();
    mSwapchainImageViews = vkbSwapchain.get_image_views().value();

    mSwapchainDeletionQueue.swapchains.push_resource(mDevice, mSwapchain, nullptr);
    for (const auto& swapchainImageView : mSwapchainImageViews)
        mSwapchainDeletionQueue.imageViews.push_resource(mDevice, swapchainImageView, nullptr);
    // Images created by the swap chain will be automatically cleaned up once it has been destroyed.
}

void VulkanEngine::destroy_swapchain()
{
    mSwapchainDeletionQueue.swapchains.flush();
    mSwapchainDeletionQueue.imageViews.flush();
}

void VulkanEngine::resize_swapchain()
{
    vkDeviceWaitIdle(mDevice);

    destroy_swapchain();

    int w, h;
    SDL_GetWindowSize(mWindow, &w, &h);
    mWindowExtent.width = w;
    mWindowExtent.height = h;

    create_swapchain(mWindowExtent.width, mWindowExtent.height);

    mResizeRequested = false;
}

VkPipelineCacheCreateInfo VulkanEngine::read_pipeline_cache(const std::string& filename)
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    std::ifstream pipelineCacheFile(filename, std::ios::binary | std::ios::ate);
    if (!pipelineCacheFile.is_open()) {
        fmt::println("Failed to open {}. Returning default VkPipelineCacheCreateInfo.", filename);
        return pipelineCacheCreateInfo;
    }

    const std::streamsize pipelineCacheSize = pipelineCacheFile.tellg();
    if (pipelineCacheSize == -1) {
        throw std::runtime_error(fmt::format("Failed to determine {} size.", filename));
    }
    mPipelineCacheData.resize(pipelineCacheSize);

    pipelineCacheFile.seekg(0);
    if (!pipelineCacheFile.read(mPipelineCacheData.data(), pipelineCacheSize)) {
        throw std::runtime_error(fmt::format("Failed to read {}.", filename));
    }
    pipelineCacheCreateInfo.pInitialData = mPipelineCacheData.data();
    pipelineCacheCreateInfo.initialDataSize = pipelineCacheSize;

    return pipelineCacheCreateInfo;
}

void VulkanEngine::write_pipeline_cache(const std::string& filename)
{
    size_t dataSize;
    vkGetPipelineCacheData(mDevice, mPipelineCache, &dataSize, nullptr); // Get size first
    vkGetPipelineCacheData(mDevice, mPipelineCache, &dataSize, mPipelineCacheData.data()); // Then read the size of data

    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    if (file.is_open()) {
        file.write(mPipelineCacheData.data(), static_cast<long long>(mPipelineCacheData.size()));
        file.close();
        fmt::println("Pipeline cache successfully written to {}.", filename);
    } else {
        throw std::runtime_error(fmt::format("Failed to write pipeline cache data to {}.", filename));
    }
}

MaterialPipeline VulkanEngine::create_pipeline(bool doubleSided, fastgltf::AlphaMode alphaMode)
{
    std::size_t optionsHash = (std::hash<bool> {}(doubleSided)) ^ ((std::hash<fastgltf::AlphaMode> {}(alphaMode)) << 1);

    if (mPipelinesCreated.contains(optionsHash)) {
        return mPipelinesCreated[optionsHash];
    }

    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module("../../shaders/mesh.frag.spv", mDevice, &meshFragShader))
        fmt::println("Error when building the triangle fragment shader module");
    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module("../../shaders/mesh.vert.spv", mDevice, &meshVertexShader))
        fmt::println("Error when building the triangle vertex shader module");

    VkPushConstantRange ssboAddressesRange {};
    ssboAddressesRange.offset = 0;
    ssboAddressesRange.size = sizeof(SSBOAddresses);
    ssboAddressesRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::vector<VkDescriptorSetLayout> layouts = { mMaterialTexturesArray.layout };
    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.pSetLayouts = layouts.data();
    mesh_layout_info.setLayoutCount = layouts.size();
    mesh_layout_info.pPushConstantRanges = &ssboAddressesRange;
    mesh_layout_info.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(mDevice, &mesh_layout_info, nullptr, &newLayout));

    VkCullModeFlags cullMode;
    (doubleSided) ? (cullMode = VK_CULL_MODE_NONE) : (cullMode = VK_CULL_MODE_BACK_BIT);
    bool transparency;
    (alphaMode == fastgltf::AlphaMode::Blend) ? (transparency = true) : (transparency = false);

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(cullMode, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.set_color_attachment_format(mDrawImage.imageFormat);
    pipelineBuilder.set_depth_format(mDepthImage.imageFormat);
    if (transparency) {
        pipelineBuilder.enable_blending_additive();
        pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    }
    pipelineBuilder.mPipelineLayout = newLayout;

    MaterialPipeline materialPipeline(pipelineBuilder.build_pipeline(mDevice) , newLayout);

    vkDestroyShaderModule(mDevice, meshFragShader, nullptr);
    vkDestroyShaderModule(mDevice, meshVertexShader, nullptr);
    mPipelineDeletionQueue.pipelines.push_resource(mDevice, materialPipeline.pipeline, nullptr);
    mPipelineDeletionQueue.pipelineLayouts.push_resource(mDevice, materialPipeline.layout, nullptr);

    mPipelinesCreated[optionsHash] = materialPipeline;

    return materialPipeline;
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, DeletionQueue<VkBuffer>& bufferDeletionQueue)
{
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    AllocatedBuffer newBuffer;
    VK_CHECK(vmaCreateBuffer(mAllocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
        &newBuffer.info));

    destroy_buffer(newBuffer, bufferDeletionQueue);

    return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer, DeletionQueue<VkBuffer>& bufferDeletionQueue) const
{
    bufferDeletionQueue.push_resource(mDevice, buffer.buffer, nullptr, mAllocator, buffer.allocation);
}

AllocatedImage VulkanEngine::create_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = extent;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, extent);
    if (mipmapped)
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY; // Always allocate images on dedicated GPU memory
    allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(mAllocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // If depth format, use the correct aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT)
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = img_info.mipLevels;
    VK_CHECK(vkCreateImageView(mDevice, &view_info, nullptr, &newImage.imageView));

    destroy_image(newImage);

    return newImage;
}

AllocatedImage VulkanEngine::create_image(const void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    const size_t dataSize = static_cast<size_t>(extent.depth) * extent.width * extent.height * 4; // TODO check dataSize below MAX_IMAGE_SIZE

    static const AllocatedBuffer stagingBuffer = create_staging_buffer(MAX_IMAGE_SIZE, mBufferDeletionQueue.lifetimeBuffers);
    static void* stagingAddress = stagingBuffer.allocation->GetMappedData();

    memcpy(stagingAddress, data, dataSize); 

    // Image to hold data loaded from file
    const AllocatedImage newImage = create_image(extent, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    // Transition image to transfer dst optimal to copy from buffer, then transition to shader read only.
    immediate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(cmd, newImage.image,
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = extent;

        vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
            &copyRegion);

        if (mipmapped)
            vkutil::generate_mipmaps(cmd, newImage.image, VkExtent2D { newImage.imageExtent.width, newImage.imageExtent.height });
        else
            vkutil::transition_image(cmd, newImage.image,
                VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR,
                VK_ACCESS_2_MEMORY_READ_BIT,
                VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR,
                VK_ACCESS_2_MEMORY_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    return newImage;
}

void VulkanEngine::destroy_image(const AllocatedImage& img)
{
    mImageDeletionQueue.imageViews.push_resource(mDevice, img.imageView, nullptr);
    mImageDeletionQueue.images.push_resource(mDevice, img.image, nullptr, mAllocator, img.allocation);
}

AllocatedBuffer VulkanEngine::create_staging_buffer(size_t allocSize, DeletionQueue<VkBuffer>& bufferDeletionQueue)
{
    return create_buffer(allocSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_CPU_ONLY, bufferDeletionQueue);
}

void VulkanEngine::create_vertex_index_buffers()
{
    mGlobalVertexBuffer = create_buffer(DEFAULT_VERTEX_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mBufferDeletionQueue.lifetimeBuffers);
    mGlobalIndexBuffer = create_buffer(DEFAULT_INDEX_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mBufferDeletionQueue.lifetimeBuffers);
}

void VulkanEngine::create_instance_buffer()
{
    const auto instanceDataSize = MAX_INSTANCES * sizeof(InstanceData);
    mInstanceBuffer = create_buffer(instanceDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mBufferDeletionQueue.lifetimeBuffers);
}

void VulkanEngine::create_scene_buffer()
{
    mSceneBuffer = create_buffer(sizeof(SceneData), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mBufferDeletionQueue.lifetimeBuffers);
}

void VulkanEngine::create_material_constants_buffer()
{
    const auto materialConstantsDataSize = MAX_MATERIALS * sizeof(MaterialConstants);
    mMaterialConstantsBuffer = create_buffer(materialConstantsDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mBufferDeletionQueue.lifetimeBuffers);
}

void VulkanEngine::create_indirect_buffer()
{
    const auto indirectBufferSize = MAX_INDIRECT_COMMANDS * sizeof(VkDrawIndexedIndirectCommand);
    mGlobalIndirectBuffer = create_buffer(indirectBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mBufferDeletionQueue.lifetimeBuffers);
}

void VulkanEngine::upload_primitive(Primitive& primitive, std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    static const AllocatedBuffer stagingBuffer = create_staging_buffer(static_cast<size_t>(DEFAULT_VERTEX_BUFFER_SIZE) + DEFAULT_INDEX_BUFFER_SIZE, mBufferDeletionQueue.lifetimeBuffers);
    static void* stagingAddress = stagingBuffer.allocation->GetMappedData();

    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    primitive.vertexBuffer = create_buffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mBufferDeletionQueue.lifetimeBuffers);
    primitive.indexBuffer = create_buffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mBufferDeletionQueue.lifetimeBuffers);

    memcpy(stagingAddress, vertices.data(), vertexBufferSize);
    memcpy(static_cast<char*>(stagingAddress) + vertexBufferSize, indices.data(), indexBufferSize);

    VkBufferCopy vertexCopy { 0 };
    vertexCopy.dstOffset = 0;
    vertexCopy.srcOffset = 0;
    vertexCopy.size = vertexBufferSize;
    VkBufferCopy indexCopy { 0 };
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = vertexBufferSize;
    indexCopy.size = indexBufferSize;

    immediate_submit([&](const VkCommandBuffer cmd) {
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, primitive.vertexBuffer.buffer, 1, &vertexCopy);
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, primitive.indexBuffer.buffer, 1, &indexCopy);
    });
}

void VulkanEngine::update_vertex_index_buffers(AllocatedBuffer srcVertex, AllocatedBuffer dstVertex, int& vertexBufferOffset,
    AllocatedBuffer srcIndex, AllocatedBuffer dstIndex, int& indexBufferOffset) const
{
    VkBufferCopy vertexCopy {};
    vertexCopy.dstOffset = vertexBufferOffset;
    vertexCopy.srcOffset = 0;
    vertexCopy.size = srcVertex.info.size;
    VkBufferCopy indexCopy {};
    indexCopy.dstOffset = indexBufferOffset;
    indexCopy.srcOffset = 0;
    indexCopy.size = srcIndex.info.size;

    vertexBufferOffset += srcVertex.info.size;
    indexBufferOffset += srcIndex.info.size;

    immediate_submit([&](const VkCommandBuffer cmd) {
        vkCmdCopyBuffer(cmd, srcVertex.buffer, dstVertex.buffer, 1, &vertexCopy);
        vkCmdCopyBuffer(cmd, srcIndex.buffer, dstIndex.buffer, 1, &indexCopy);
    });
}

void VulkanEngine::update_indirect_commands(Primitive& primitive, int& verticesOffset, int& indicesOffset, int& primitivesOffset)
{
    VkDrawIndexedIndirectCommand indirectCmd {};
    indirectCmd.instanceCount = OBJECT_COUNT;
    indirectCmd.firstInstance = 0;
    indirectCmd.vertexOffset = verticesOffset;
    indirectCmd.indexCount = primitive.indexCount;
    indirectCmd.firstIndex = indicesOffset;

    mIndirectBatches[primitive.material.get()].push_back(indirectCmd);

    verticesOffset += primitive.vertexCount;
    indicesOffset += primitive.indexCount;
    primitivesOffset++;
}

void VulkanEngine::iterate_primitives()
{
    int vertexBufferOffset = 0;
    int indexBufferOffset = 0;

    int verticesOffset = 0;
    int indicesOffset = 0;
    int primitivesOffset = 0;

    for (const auto& model : mLoadedModels | std::views::values) {
        for (const auto& mesh : model->mMeshes | std::views::values) {
            for (auto& primitive : mesh->primitives) {
                update_vertex_index_buffers(primitive.vertexBuffer, mGlobalVertexBuffer, vertexBufferOffset, primitive.indexBuffer, mGlobalIndexBuffer, indexBufferOffset);
                update_indirect_commands(primitive, verticesOffset, indicesOffset, primitivesOffset);
            }
        }
    }
}

void VulkanEngine::update_indirect_buffer()
{
    static const AllocatedBuffer stagingBuffer = create_staging_buffer(mGlobalIndirectBuffer.info.size, mBufferDeletionQueue.lifetimeBuffers);
    static void* stagingAddress = stagingBuffer.allocation->GetMappedData();

    VkDeviceSize indirectBufferOffset = 0;
    for (const auto& indirectBatch : mIndirectBatches) {
        auto* currentMaterial = indirectBatch.first;
        const auto& indirectCommands = mIndirectBatches[currentMaterial];

        const VkDeviceSize indirectCommandsSize = indirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand);

        memcpy(static_cast<char*>(stagingAddress) + indirectBufferOffset, indirectCommands.data(), indirectCommandsSize);

        indirectBufferOffset += indirectCommandsSize;
    }

    VkBufferCopy indirectCopy {};
    indirectCopy.dstOffset = 0;
    indirectCopy.srcOffset = 0;
    indirectCopy.size = stagingBuffer.info.size;

    mBufferCopyBatches.emplace_back(
        stagingBuffer.buffer,
        mGlobalIndirectBuffer.buffer,
        std::vector<VkBufferCopy> { indirectCopy });
}

void VulkanEngine::update_instanced_buffer()
{
    static const AllocatedBuffer stagingBuffer = create_staging_buffer(mInstanceBuffer.info.size, mBufferDeletionQueue.lifetimeBuffers);
    static void* stagingAddress = stagingBuffer.allocation->GetMappedData();

    std::vector<InstanceData> instanceData;
    instanceData.resize(OBJECT_COUNT);
    for (int i = 0; i < OBJECT_COUNT; i++) {
        instanceData[i].translation = glm::translate(glm::mat4 { 1.0f }, glm::vec3 { 0, 0, 0 });
        instanceData[i].rotation = glm::toMat4(rotation(glm::vec3(), glm::vec3()));
        instanceData[i].scale = glm::scale(glm::mat4 { 1.0f }, glm::vec3 { 3.f });
    }

    const VkDeviceSize instanceDataSize = instanceData.size() * sizeof(InstanceData);

    memcpy(stagingAddress, instanceData.data(), instanceDataSize);

    VkBufferCopy instanceCopy {};
    instanceCopy.dstOffset = 0;
    instanceCopy.srcOffset = 0;
    instanceCopy.size = stagingBuffer.info.size;

    mBufferCopyBatches.emplace_back(
        stagingBuffer.buffer,
        mInstanceBuffer.buffer,
        std::vector<VkBufferCopy> { instanceCopy });
}

void VulkanEngine::update_scene_buffer()
{
    static const AllocatedBuffer stagingBuffer = create_staging_buffer(sizeof(SceneData), mBufferDeletionQueue.lifetimeBuffers);
    static void* stagingAddress = stagingBuffer.allocation->GetMappedData();

    mSceneData.ambientColor = glm::vec4(.1f);
    mSceneData.sunlightColor = glm::vec4(1.f);
    mSceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);
    mMainCamera.updatePosition(mStats.frametime, static_cast<float>(ONE_SECOND_IN_MILLISECONDS / EXPECTED_FRAME_RATE));
    mSceneData.view = mMainCamera.getViewMatrix();
    mSceneData.proj = glm::perspective(glm::radians(70.f), static_cast<float>(mWindowExtent.width) / static_cast<float>(mWindowExtent.height), 10000.f, 0.1f);
    mSceneData.proj[1][1] *= -1;

    const VkDeviceSize sceneDataSize = sizeof(SceneData);

    memcpy(stagingAddress, &mSceneData, sceneDataSize);

    VkBufferCopy sceneCopy {};
    sceneCopy.dstOffset = 0;
    sceneCopy.srcOffset = 0;
    sceneCopy.size = sceneDataSize;

    mBufferCopyBatches.emplace_back(
        stagingBuffer.buffer,
        mSceneBuffer.buffer,
        std::vector<VkBufferCopy> { sceneCopy });
}

void VulkanEngine::update_material_buffer()
{
    static const AllocatedBuffer stagingBuffer = create_staging_buffer(mMaterialConstantsBuffer.info.size, mBufferDeletionQueue.lifetimeBuffers);
    static void* stagingAddress = stagingBuffer.allocation->GetMappedData();

    VkDeviceSize materialConstantsBufferOffset = 0;
    for (const auto& indirectBatch : mIndirectBatches) {
        auto* currentMaterial = indirectBatch.first;

        const VkDeviceSize materialConstantsSize = sizeof(MaterialConstants);

        memcpy(static_cast<char*>(stagingAddress) + materialConstantsBufferOffset, &currentMaterial->mData.constants, materialConstantsSize);

        materialConstantsBufferOffset += materialConstantsSize;
    }

    VkBufferCopy materialCopy {};
    materialCopy.dstOffset = 0;
    materialCopy.srcOffset = 0;
    materialCopy.size = stagingBuffer.info.size;

    mBufferCopyBatches.emplace_back(
        stagingBuffer.buffer,
        mMaterialConstantsBuffer.buffer,
        std::vector<VkBufferCopy> { materialCopy });
}

void VulkanEngine::update_material_texture_array()
{
    DescriptorWriter writer;
    int arrayIndex = 0;

    for (const auto& indirectBatch : mIndirectBatches) {
        const auto* currentMaterial = indirectBatch.first;

        writer.write_image_array(0, currentMaterial->mData.resources.base.image.imageView, currentMaterial->mData.resources.base.sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arrayIndex);
        writer.write_image_array(0, currentMaterial->mData.resources.emissive.image.imageView, currentMaterial->mData.resources.emissive.sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arrayIndex + 1);
        writer.write_image_array(0, currentMaterial->mData.resources.metallicRoughness.image.imageView, currentMaterial->mData.resources.metallicRoughness.sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arrayIndex + 2);
        writer.write_image_array(0, currentMaterial->mData.resources.normal.image.imageView, currentMaterial->mData.resources.normal.sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arrayIndex + 3);
        writer.write_image_array(0, currentMaterial->mData.resources.occlusion.image.imageView, currentMaterial->mData.resources.occlusion.sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arrayIndex + 4);

        arrayIndex += 5;
    }

    writer.update_set(mDevice, mMaterialTexturesArray.set);
}

void VulkanEngine::submit_buffer_updates() const
{
    immediate_submit([&](const VkCommandBuffer cmd) {
        for (const auto& bufferCopyBatch : mBufferCopyBatches) {
            vkCmdCopyBuffer(cmd, bufferCopyBatch.srcBuffer, bufferCopyBatch.dstBuffer, bufferCopyBatch.bufferCopies.size(), bufferCopyBatch.bufferCopies.data());
        }
    });
}

void VulkanEngine::update_draw_data()
{
    const auto start = std::chrono::system_clock::now();

    iterate_primitives();
    update_indirect_buffer();
    update_material_buffer();
    update_material_texture_array();
    update_instanced_buffer();
    update_scene_buffer();
    submit_buffer_updates();

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.scene_update_time = static_cast<float>(elapsed.count()) / 1000.f;
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) const
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    const VkRenderingInfo renderInfo = vkinit::rendering_info(mSwapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
    mStats.drawcall_count = 0;
    mStats.triangle_count = 0;
    auto start = std::chrono::system_clock::now();

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(mDrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(mDepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = vkinit::rendering_info(mDrawExtent, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(mDrawExtent.width);
    viewport.height = static_cast<float>(mDrawExtent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = mDrawExtent.width;
    scissor.extent.height = mDrawExtent.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindIndexBuffer(cmd, mGlobalIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    VkDeviceSize indirectBufferOffset = 0;
    for (const auto& indirectBatch : mIndirectBatches) {
        PbrMaterial* currentMaterial = indirectBatch.first;
        indirectBufferOffset += indirectBatch.second.size() * sizeof(VkDrawIndexedIndirectCommand);

        vkCmdPushConstants(cmd, currentMaterial->mPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SSBOAddresses), &mPushConstants);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentMaterial->mPipeline.pipeline); // TODO Check for same pipeline in previous loop
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentMaterial->mPipeline.layout, 0, 1, &mMaterialTexturesArray.set, 0, nullptr);

        vkCmdDrawIndexedIndirect(cmd, mGlobalIndirectBuffer.buffer, indirectBufferOffset, indirectBatch.second.size(), sizeof(VkDrawIndexedIndirectCommand));

        mStats.drawcall_count++;
    }

    vkCmdEndRendering(cmd);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mesh_draw_time = static_cast<float>(elapsed.count()) / 1000.f;
}

void VulkanEngine::draw()
{
    // Wait until the gpu has finished rendering the frame of this index (become signalled), or until timeout of 1 second (in nanoseconds).
    VK_CHECK(vkWaitForFences(mDevice, 1, &(get_current_frame().mRenderFence), true, 1000000000));
    VK_CHECK(vkResetFences(mDevice, 1, &(get_current_frame().mRenderFence))); // Flip to unsignalled

    cleanup_per_draw();
    update_draw_data();

    // Request image from the swapchain
    // _swapchainSemaphore signalled only when next image is acquired.
    uint32_t swapchainImageIndex;
    if (vkAcquireNextImageKHR(mDevice, mSwapchain, 1000000000, get_current_frame().mSwapchainSemaphore, nullptr, &swapchainImageIndex) == VK_ERROR_OUT_OF_DATE_KHR) {
        mResizeRequested = true;
        return;
    }

    const VkCommandBuffer cmd = get_current_frame().mMainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0)); // Reset the command buffer to begin recording again for each frame.
    const VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo)); // Start the command buffer recording

    // Multiply by render scale for dynamic resolution
    // When resizing bigger, don't make swapchain extent go beyond draw image extent
    mDrawExtent.height = std::min(mSwapchainExtent.height, mDrawImage.imageExtent.height) * mRenderScale;
    mDrawExtent.width = std::min(mSwapchainExtent.width, mDrawImage.imageExtent.width) * mRenderScale;

    // Transition stock and draw image into transfer layouts
    vkutil::transition_image(cmd, mStockImages["blue"].image,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, mDrawImage.image,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy stock image as the initial colour for the draw image (the background)
    vkutil::copy_image_to_image(cmd, mStockImages["blue"].image, mDrawImage.image, VkExtent2D {
                                                                                       mStockImages["blue"].imageExtent.width,
                                                                                       mStockImages["blue"].imageExtent.height,
                                                                                   },
        mDrawExtent);

    // Transition to color output for drawing coloured triangle
    vkutil::transition_image(cmd, mDrawImage.image,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transition_image(cmd, mDepthImage.image,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    draw_geometry(cmd);

    // Transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(cmd, mDrawImage.image,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, mSwapchainImages[swapchainImageIndex],
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy draw image into the swapchain image
    vkutil::copy_image_to_image(cmd, mDrawImage.image, mSwapchainImages[swapchainImageIndex], mDrawExtent, mSwapchainExtent);

    // Set swapchain image to be attachment optimal to draw it
    vkutil::transition_image(cmd, mSwapchainImages[swapchainImageIndex],
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Draw imgui into the swapchain image
    draw_imgui(cmd, mSwapchainImageViews[swapchainImageIndex]);

    // Set swapchain image layout to presentable layout
    vkutil::transition_image(cmd, mSwapchainImages[swapchainImageIndex],
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // Finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Prepare the submission to the queue. (Reading semaphore states)
    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().mSwapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().mRenderSemaphore);
    const VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    // Submit command buffer to the queue and execute it.
    // _renderFence will block CPU from going to next frame, stays unsignalled until this is done.
    // _swapchainSemaphore gets waited on until it is signalled when the next image is acquired.
    // _renderSemaphore will be signalled by this function when this queue's commands are executed.
    VK_CHECK(vkQueueSubmit2(mGraphicsQueue, 1, &submit, get_current_frame().mRenderFence));

    // Prepare present.
    // Wait on the _renderSemaphore for queue commands to finish before image is presented.
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &mSwapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &get_current_frame().mRenderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex;

    if (vkQueuePresentKHR(mGraphicsQueue, &presentInfo) == VK_ERROR_OUT_OF_DATE_KHR)
        mResizeRequested = true;

    mFrameNumber++;
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    while (!bQuit) {
        auto start = std::chrono::system_clock::now();

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT)
                bQuit = true;
            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
                    mStopRendering = true;
                if (e.window.event == SDL_WINDOWEVENT_RESTORED)
                    mStopRendering = false;
            }
            mMainCamera.processSDLEvent(e);
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        // Do not draw if we are minimized
        if (mStopRendering) {
            // Throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Misc options
        SDL_SetRelativeMouseMode(mMainCamera.relativeMode);
        if (mResizeRequested)
            resize_swapchain();

        // Imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(mWindow);
        ImGui::NewFrame();
        if (ImGui::Begin("Background")) {
            ImGui::SliderFloat("Render Scale", &mRenderScale, 0.3f, 1.f);
            ImGui::Text("[F1] Camera Mode: %s", magic_enum::enum_name(mMainCamera.movementMode).data());
            ImGui::Text("[F2] Mouse Mode: %s", (mMainCamera.relativeMode ? "RELATIVE" : "NORMAL"));
            ImGui::End();
        }
        if (ImGui::Begin("Stats")) {
            ImGui::Text("Compile Mode: %s", (bUseValidationLayers ? "DEBUG" : "RELEASE"));
            ImGui::Text("Frame Time:  %fms", mStats.frametime);
            ImGui::Text("Draw Time: %fms", mStats.mesh_draw_time);
            ImGui::Text("Update Time: %fms", mStats.scene_update_time);
            ImGui::Text("Triangles: %i", mStats.triangle_count);
            ImGui::Text("Draws: %i", mStats.drawcall_count);
            ImGui::End();
        }
        ImGui::Render();

        draw();

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        mStats.frametime = static_cast<float>(elapsed.count()) / 1000.f;
    }
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) const
{
    VK_CHECK(vkResetFences(mDevice, 1, &mImmSubmit.fence));
    VK_CHECK(vkResetCommandBuffer(mImmSubmit.commandBuffer, 0));

    const VkCommandBuffer cmd = mImmSubmit.commandBuffer;

    const VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
    function(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    const VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(mGraphicsQueue, 1, &submit, mImmSubmit.fence));
    VK_CHECK(vkWaitForFences(mDevice, 1, &mImmSubmit.fence, true, 9999999999));
}

void VulkanEngine::cleanup_immediate()
{
    mImmediateDeletionQueue.fences.flush();
    mImmediateDeletionQueue.commandPools.flush();
}

void VulkanEngine::cleanup_swapchain()
{
    destroy_swapchain();
}

void VulkanEngine::cleanup_descriptors()
{
    mGlobalDescriptorAllocator.destroy_pools(mDevice);
    mDescriptorDeletionQueue.descriptorSetLayouts.flush();
}

void VulkanEngine::cleanup_pipeline_caches()
{
    write_pipeline_cache(pipelineCacheFile);
    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
}

void VulkanEngine::cleanup_pipelines()
{
    mPipelineDeletionQueue.pipelines.flush();
    mPipelineDeletionQueue.pipelineLayouts.flush();
}

void VulkanEngine::cleanup_samplers()
{
    mSamplerDeletionQueue.samplers.flush();
}

void VulkanEngine::cleanup_images()
{
    mImageDeletionQueue.images.flush();
    mImageDeletionQueue.imageViews.flush();
}

void VulkanEngine::cleanup_buffers()
{
    mBufferDeletionQueue.lifetimeBuffers.flush();
    mBufferDeletionQueue.perDrawBuffers.flush();
    mBufferDeletionQueue.tempBuffers.flush();
}

void VulkanEngine::cleanup_imgui() const
{
    vkDestroyDescriptorPool(mDevice, mImmSubmit.imguiDescriptorPool, nullptr);
    ImGui_ImplVulkan_Shutdown();
}

void VulkanEngine::cleanup_misc() const
{
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    vkb::destroy_debug_utils_messenger(mInstance, mDebugMessenger);
}

void VulkanEngine::cleanup_per_draw()
{
    get_current_frame().mFrameDescriptors.clear_pools(mDevice);
    get_current_frame().mFrameDeletionQueue.bufferDeletion.flush(); // For buffers that are used by cmd buffers. Wait for fence of this frame to be reset before flushing.
    mBufferDeletionQueue.perDrawBuffers.flush();
    mIndirectBatches.clear();
    mBufferCopyBatches.clear(); 
}

void VulkanEngine::cleanup_core() const
{
    vmaDestroyAllocator(mAllocator);
    vkDestroyDevice(mDevice, nullptr);
    vkDestroyInstance(mInstance, nullptr);
    SDL_DestroyWindow(mWindow);
}

void VulkanEngine::cleanup()
{
    if (mIsInitialized) {
        vkDeviceWaitIdle(mDevice);

        // GLTF scenes cleared by own destructor.
        mLoadedModels.clear();
        for (FrameData& frame : mFrames)
            frame.cleanup(mDevice);
        cleanup_immediate();
        cleanup_swapchain();
        cleanup_descriptors();
        cleanup_pipelines();
        cleanup_samplers();
        cleanup_images();
        cleanup_buffers();
        cleanup_imgui();
        cleanup_misc();
        cleanup_core();

        // Clear engine pointer
        loadedEngine = nullptr;
    }
}
