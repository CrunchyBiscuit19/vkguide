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

constexpr int objectCount = 1;
const std::string pipelineCacheFile = "../../bin/pipeline_cache.bin";
// const std::vector<std::string> modelFilepaths { "../../assets/AntiqueCamera/AntiqueCamera.glb" };
const std::vector<std::string> modelFilepaths { "../../assets/toycar/ToyCar.glb" };

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

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        static_cast<int>(_windowExtent.width),
        static_cast<int>(_windowExtent.height),
        window_flags);

    init_vulkan();
    init_swapchain();
    init_commands();
    init_sync_structures();
    init_descriptors();
    init_pipelines();
    init_imgui();
    init_default_data();
    init_models(modelFilepaths);
    mainCamera.init();

    // Everything went fine
    _isInitialized = true;
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

    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &_imguiDescriptorPool));

    // Initialize imgui library
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(_window);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _chosenGPU;
    init_info.Device = _device;
    init_info.Queue = _graphicsQueue;
    init_info.DescriptorPool = _imguiDescriptorPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.ColorAttachmentFormat = _swapchainImageFormat;
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

    _instance = vkb_inst.instance;
    _debugMessenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

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
                                             .set_surface(_surface)
                                             .select()
                                             .value();
    vkb::DeviceBuilder deviceBuilder { physicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    _device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

    // Get a graphics queue / family
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // Initialize the memory _allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosenGPU;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void VulkanEngine::init_swapchain()
{
    create_swapchain(_windowExtent.width, _windowExtent.height);

    // draw image size will match the window
    const VkExtent3D drawImageExtent = {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    // Hardcoding the draw format to 16 bit floats, extra precision for lighting calculations and better rendering.
    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages {};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

    // Allocate draw image from GPU local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

    // Build a image-view for the draw image to use for rendering
    const VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages {};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    const VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);
    vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

    const VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));

    destroy_image(_drawImage);
    destroy_image(_depthImage);
}

void VulkanEngine::init_commands()
{
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    const VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // One command pool and command buffer per frame stored
    for (FrameData& frame : _frames) {
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &frame._commandPool));
        // Allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(frame._commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &frame._mainCommandBuffer));

        frame._frameDeletionQueue.commandPoolDeletion.push_resource(_device, frame._commandPool, nullptr);
    }

    // Immediate submits
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));
    const VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

    _immediateDeletionQueue.commandPools.push_resource(_device, _immCommandPool, nullptr);
}

void VulkanEngine::init_sync_structures()
{
    // 1 fence to control when the gpu has finished rendering the frame,
    // 2 semaphores to syncronize rendering with swapchain
    // Fence to start signalled so we can wait on it on the first frame
    const VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (FrameData& frame : _frames) {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &frame._renderFence));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &frame._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &frame._renderSemaphore));

        frame._frameDeletionQueue.fenceDeletion.push_resource(_device, frame._renderFence, nullptr);
        frame._frameDeletionQueue.semaphoreDeletion.push_resource(_device, frame._swapchainSemaphore, nullptr);
        frame._frameDeletionQueue.semaphoreDeletion.push_resource(_device, frame._renderSemaphore, nullptr);
    }

    // Immediate fence
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
    _immediateDeletionQueue.fences.push_resource(_device, _immFence, nullptr);
}

void VulkanEngine::init_descriptors()
{
    // Create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10000 },
    };
    _globalDescriptorAllocator.init(_device, 10, sizes);

    // Create descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    // Allocate a descriptor set for our draw image
    _drawImageDescriptors = _globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);
    DescriptorWriter writer;
    writer.write_image(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(_device, _drawImageDescriptors);

    // Create descriptor set layout for texture array
    int materialTexturesArraySize = 1000;
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, materialTexturesArraySize);
        materialTexturesArraySetLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT, true);
    }
    // Allocate a descriptor set for texture array
    materialTexturesArrayDescriptorSet = _globalDescriptorAllocator.allocate(_device, materialTexturesArraySetLayout, true, materialTexturesArraySize);

    // Create descriptor set layout for the magenta-black texture
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        _stockImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        _frames[i]._frameDescriptors = DescriptorAllocatorGrowable {};
        _frames[i]._frameDescriptors.init(_device, 1000, frame_sizes);
    }

    _descriptorDeletionQueue.descriptorSetLayouts.push_resource(_device, _stockImageDescriptorLayout, nullptr);
    _descriptorDeletionQueue.descriptorSetLayouts.push_resource(_device, _drawImageDescriptorLayout, nullptr);
    _descriptorDeletionQueue.descriptorSetLayouts.push_resource(_device, materialTexturesArraySetLayout, nullptr);
}

void VulkanEngine::init_pipeline_caches()
{
    const VkPipelineCacheCreateInfo pipelineCacheCreateInfo = read_pipeline_cache(pipelineCacheFile);
    VK_CHECK(vkCreatePipelineCache(_device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanEngine::init_pipelines()
{
}

void VulkanEngine::init_default_data()
{
    // Colour data interpreted as little endian
    constexpr uint32_t white = std::byteswap(0xFFFFFFFF);
    _stockImages["white"] = create_image(&white, VkExtent3D { 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    constexpr uint32_t grey = std::byteswap(0xAAAAAAFF);
    _stockImages["grey"] = create_image(&grey, VkExtent3D { 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    constexpr uint32_t black = std::byteswap(0x000000FF);
    _stockImages["black"] = create_image(&black, VkExtent3D { 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    constexpr uint32_t blue = std::byteswap(0x769DDBFF);
    _stockImages["blue"] = create_image(&blue, VkExtent3D { 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    // 16x16 checkerboard texture
    std::array<uint32_t, 16 * 16> pixels;
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            // constexpr uint32_t magenta = 0xFF00FFFF;
            constexpr uint32_t magenta = std::byteswap(0xFF00FFFF);
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    _stockImages["errorCheckerboard"] = create_image(pixels.data(), VkExtent3D { 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    // Default samplers
    VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest);
    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear);

    _samplerDeletionQueue.samplers.push_resource(_device, _defaultSamplerLinear, nullptr);
    _samplerDeletionQueue.samplers.push_resource(_device, _defaultSamplerNearest, nullptr);
}

void VulkanEngine::init_models(const std::vector<std::string>& modelPaths)
{
    for (const auto& modelPath : modelPaths) {
        const auto modelFile = load_gltf(this, modelPath);
        assert(modelFile.has_value());
        loadedModels[std::filesystem::path(modelPath).filename().string()] = *modelFile;
    }
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder { _chosenGPU, _device, _surface };

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM; // Why is this BGRA?
    vkb::Swapchain vkbSwapchain = swapchainBuilder
                                      //.use_default_format_selection()
                                      .set_desired_format(VkSurfaceFormatKHR { .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
                                      // use vsync present mode
                                      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                      .set_desired_extent(width, height)
                                      .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                      .build()
                                      .value();

    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();

    _swapchainDeletionQueue.swapchains.push_resource(_device, _swapchain, nullptr);
    for (const auto& swapchainImageView : _swapchainImageViews)
        _swapchainDeletionQueue.imageViews.push_resource(_device, swapchainImageView, nullptr);
    // Images created by the swap chain will be automatically cleaned up once it has been destroyed.
}

void VulkanEngine::destroy_swapchain()
{
    _swapchainDeletionQueue.swapchains.flush();
    _swapchainDeletionQueue.imageViews.flush();
}

void VulkanEngine::resize_swapchain()
{
    vkDeviceWaitIdle(_device);

    destroy_swapchain();

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;

    create_swapchain(_windowExtent.width, _windowExtent.height);

    _resize_requested = false;
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
    pipelineCacheData.resize(pipelineCacheSize);

    pipelineCacheFile.seekg(0);
    if (!pipelineCacheFile.read(pipelineCacheData.data(), pipelineCacheSize)) {
        throw std::runtime_error(fmt::format("Failed to read {}.", filename));
    }
    pipelineCacheCreateInfo.pInitialData = pipelineCacheData.data();
    pipelineCacheCreateInfo.initialDataSize = pipelineCacheSize;

    return pipelineCacheCreateInfo;
}

void VulkanEngine::write_pipeline_cache(const std::string& filename)
{
    size_t dataSize;
    vkGetPipelineCacheData(_device, pipelineCache, &dataSize, nullptr); // Get size first
    vkGetPipelineCacheData(_device, pipelineCache, &dataSize, pipelineCacheData.data()); // Then read the size of data

    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    if (file.is_open()) {
        file.write(pipelineCacheData.data(), static_cast<long long>(pipelineCacheData.size()));
        file.close();
        fmt::println("Pipeline cache successfully written to {}.", filename);
    } else {
        throw std::runtime_error(fmt::format("Failed to write pipeline cache data to {}.", filename));
    }
}

MaterialPipeline VulkanEngine::create_pipeline(bool doubleSided, fastgltf::AlphaMode alphaMode)
{
    std::size_t optionsHash = (std::hash<bool> {}(doubleSided)) ^ ((std::hash<fastgltf::AlphaMode> {}(alphaMode)) << 1);

    if (pipelinesCreated.contains(optionsHash)) {
        return pipelinesCreated[optionsHash];
    }

    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module("../../shaders/mesh.frag.spv", _device, &meshFragShader))
        fmt::println("Error when building the triangle fragment shader module");
    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module("../../shaders/mesh.vert.spv", _device, &meshVertexShader))
        fmt::println("Error when building the triangle vertex shader module");

    VkPushConstantRange ssboAddressesRange {};
    ssboAddressesRange.offset = 0;
    ssboAddressesRange.size = sizeof(SSBOAddresses);
    ssboAddressesRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::vector<VkDescriptorSetLayout> layouts = { materialTexturesArraySetLayout };
    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.pSetLayouts = layouts.data();
    mesh_layout_info.setLayoutCount = layouts.size();
    mesh_layout_info.pPushConstantRanges = &ssboAddressesRange;
    mesh_layout_info.pushConstantRangeCount = 1;

    MaterialPipeline materialPipeline;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(_device, &mesh_layout_info, nullptr, &newLayout));
    materialPipeline.layout = newLayout;

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
    pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(_depthImage.imageFormat);
    if (transparency) {
        pipelineBuilder.enable_blending_additive();
        pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    }
    pipelineBuilder._pipelineLayout = newLayout;

    materialPipeline.pipeline = pipelineBuilder.build_pipeline(_device);

    vkDestroyShaderModule(_device, meshFragShader, nullptr);
    vkDestroyShaderModule(_device, meshVertexShader, nullptr);
    _pipelineDeletionQueue.pipelines.push_resource(_device, materialPipeline.pipeline, nullptr);
    _pipelineDeletionQueue.pipelineLayouts.push_resource(_device, materialPipeline.layout, nullptr);

    pipelinesCreated[optionsHash] = materialPipeline;

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
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
        &newBuffer.info));

    destroy_buffer(newBuffer, bufferDeletionQueue);

    return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer, DeletionQueue<VkBuffer>& bufferDeletionQueue)
{
    bufferDeletionQueue.push_resource(_device, buffer.buffer, nullptr, _allocator, buffer.allocation);
}

AllocatedImage VulkanEngine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped)
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY; // Always allocate images on dedicated GPU memory
    allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // If depth format, use the correct aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT)
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = img_info.mipLevels;
    VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

    destroy_image(newImage);

    return newImage;
}

AllocatedImage VulkanEngine::create_image(const void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    const size_t dataSize = size.depth * size.width * size.height * 4;

    const AllocatedBuffer stagingBuffer = create_buffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, _bufferDeletionQueue.tempBuffers); // Staging buffer
    void* stagingAddress = stagingBuffer.allocation->GetMappedData();
    memcpy(stagingAddress, data, dataSize);

    // Image to hold data loaded from file
    const AllocatedImage newImage = create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

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
        copyRegion.imageExtent = size;

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

    _bufferDeletionQueue.tempBuffers.flush();

    return newImage;
}

void VulkanEngine::destroy_image(const AllocatedImage& img)
{
    _imageDeletionQueue.imageViews.push_resource(_device, img.imageView, nullptr);
    _imageDeletionQueue.images.push_resource(_device, img.image, nullptr, _allocator, img.allocation);
}

MeshBuffers VulkanEngine::upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    // Unefficient pattern of waiting for the GPU command to fully execute before continuing with CPU logic

    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    MeshBuffers newSurface;

    newSurface.vertexCount = vertices.size();
    newSurface.indexCount = indices.size();

    newSurface.vertexBuffer = create_buffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, _bufferDeletionQueue.genericBuffers);
    newSurface.indexBuffer = create_buffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, _bufferDeletionQueue.genericBuffers);

    const AllocatedBuffer stagingBuffer = create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, _bufferDeletionQueue.tempBuffers);
    void* stagingAddress = stagingBuffer.allocation->GetMappedData();
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
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    return newSurface;
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) const
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    const VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
    stats.drawcall_count = 0;
    stats.triangle_count = 0;
    auto start = std::chrono::system_clock::now();

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);

    // Push constants of Vertex, Instance, and Scene data buffer addresses
    SSBOAddresses pushConstants;
    VkBufferDeviceAddressInfo deviceAddressInfo { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = indirectVertexBuffer.buffer };
    pushConstants.vertexBuffer = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);
    deviceAddressInfo.buffer = instanceBuffer.buffer;
    pushConstants.instanceBuffer = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);
    deviceAddressInfo.buffer = sceneBuffer.buffer;
    pushConstants.sceneBuffer = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

    // Index buffer of all models in the scene
    vkCmdBindIndexBuffer(cmd, indirectIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    for (const auto& indirectBatch : indirectBatches) {
        PbrMaterial* currentMaterial = indirectBatch.first;

        create_material_buffer(*currentMaterial);
        create_material_texture_array(*currentMaterial);
        deviceAddressInfo.buffer = materialConstantsBuffer.buffer;
        pushConstants.materialsBuffer = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentMaterial->pipeline.pipeline); // TODO Check for same pipeline in previous loop
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentMaterial->pipeline.layout, 0, 1, &materialTexturesArrayDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, currentMaterial->pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SSBOAddresses), &pushConstants);

        // Set dynamic viewport
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = static_cast<float>(_drawExtent.width);
        viewport.height = static_cast<float>(_drawExtent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        // Set dynamic scissor
        VkRect2D scissor = {};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = _drawExtent.width;
        scissor.extent.height = _drawExtent.height;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdDrawIndexedIndirect(cmd, indirectBuffers[currentMaterial].buffer, 0, indirectBatch.second.size(), sizeof(VkDrawIndexedIndirectCommand));
        stats.drawcall_count++;
    }

    vkCmdEndRendering(cmd);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.mesh_draw_time = static_cast<float>(elapsed.count()) / 1000.f;
}

void VulkanEngine::create_vertex_index_buffers()
{
    int totalVertexSize = 0;
    int totalIndexSize = 0;
    for (const auto& model : loadedModels | std::views::values) {
        for (const auto& mesh : model->meshes | std::views::values) {
            totalVertexSize += mesh->meshBuffers.vertexBuffer.info.size;
            totalIndexSize += mesh->meshBuffers.indexBuffer.info.size;
        }
    }

    indirectVertexBuffer = create_buffer(totalVertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, _bufferDeletionQueue.perDrawBuffers);
    indirectIndexBuffer = create_buffer(totalIndexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, get_current_frame()._frameDeletionQueue.bufferDeletion);

    int currentVertexOffset = 0;
    int currentIndexOffset = 0;
    for (const auto& model : loadedModels | std::views::values) {
        for (const auto& mesh : model->meshes | std::views::values) {
            VkBufferCopy vertexCopy {};
            vertexCopy.dstOffset = currentVertexOffset;
            vertexCopy.srcOffset = 0;
            vertexCopy.size = mesh->meshBuffers.vertexBuffer.info.size;
            VkBufferCopy indexCopy {};
            indexCopy.dstOffset = currentIndexOffset;
            indexCopy.srcOffset = 0;
            indexCopy.size = mesh->meshBuffers.indexBuffer.info.size;

            immediate_submit([&](const VkCommandBuffer cmd) {
                vkCmdCopyBuffer(cmd, mesh->meshBuffers.vertexBuffer.buffer, indirectVertexBuffer.buffer, 1, &vertexCopy);
                vkCmdCopyBuffer(cmd, mesh->meshBuffers.indexBuffer.buffer, indirectIndexBuffer.buffer, 1, &indexCopy);
            });

            currentVertexOffset += mesh->meshBuffers.vertexBuffer.info.size;
            currentIndexOffset += mesh->meshBuffers.indexBuffer.info.size;
        }
    }
}

void VulkanEngine::create_indirect_commands()
{
    // Batch one vector of indirect draw commands per material
    int totalPrimitives = 0;
    int totalVertex = 0;
    for (const auto& model : loadedModels | std::views::values) {

        VkDrawIndexedIndirectCommand indirectCmd {};
        indirectCmd.instanceCount = objectCount;
        indirectCmd.firstInstance = totalPrimitives * objectCount;

        for (const auto& mesh : model->meshes | std::views::values) {
            for (const auto& primitive : mesh->primitives) {
                indirectCmd.vertexOffset = totalVertex;
                indirectCmd.indexCount = primitive.indexCount;
                indirectCmd.firstIndex = primitive.firstIndex; // TODO move from loader to when we building vertex and index buffer

                indirectBatches[primitive.material.get()].push_back(indirectCmd);

                totalVertex += primitive.vertexCount;
                totalPrimitives++;
            }
        }
    }

    // Create one indirect buffer for each material based on associated indirect draw commands
    for (const auto& indirectBatch : indirectBatches) {
        const auto& indirectCommand = indirectBatch.second;
        const auto indirectCommandSize = indirectCommand.size() * sizeof(VkDrawIndexedIndirectCommand);

        const AllocatedBuffer stagingBuffer = create_buffer(indirectCommandSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_CPU_ONLY, get_current_frame()._frameDeletionQueue.bufferDeletion);
        void* stagingAddress = stagingBuffer.allocation->GetMappedData();
        memcpy(stagingAddress, indirectCommand.data(), indirectCommandSize);

        indirectBuffers[indirectBatch.first] = create_buffer(indirectCommandSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, get_current_frame()._frameDeletionQueue.bufferDeletion);
        VkBufferCopy indirectCopy {};
        indirectCopy.dstOffset = 0;
        indirectCopy.srcOffset = 0;
        indirectCopy.size = indirectCommandSize;

        immediate_submit([&](const VkCommandBuffer cmd) {
            vkCmdCopyBuffer(cmd, stagingBuffer.buffer, indirectBuffers[indirectBatch.first].buffer, 1, &indirectCopy);
        });
    }
}

void VulkanEngine::create_instanced_data()
{
    std::vector<InstanceData> instanceData;
    instanceData.resize(objectCount);
    for (int i = 0; i < objectCount; i++) {
        const int column = i / 100;
        const int row = i % 100;
        instanceData[i].translation = glm::translate(glm::mat4 { 1.0f }, glm::vec3 { column, 0, row });
        instanceData[i].rotation = glm::toMat4(rotation(glm::vec3(), glm::vec3()));
        instanceData[i].scale = glm::scale(glm::mat4 { 1.0f }, glm::vec3 { 0.2f });
    }

    const auto instanceDataSize = instanceData.size() * sizeof(InstanceData);

    const AllocatedBuffer stagingBuffer = create_buffer(instanceDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_CPU_ONLY, _bufferDeletionQueue.perDrawBuffers);
    void* stagingAddress = stagingBuffer.allocation->GetMappedData();
    memcpy(stagingAddress, instanceData.data(), instanceDataSize);

    instanceBuffer = create_buffer(instanceDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, _bufferDeletionQueue.perDrawBuffers);
    VkBufferCopy instanceCopy {};
    instanceCopy.dstOffset = 0;
    instanceCopy.srcOffset = 0;
    instanceCopy.size = instanceDataSize;

    immediate_submit([&](const VkCommandBuffer cmd) {
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, instanceBuffer.buffer, 1, &instanceCopy);
    });
}

void VulkanEngine::create_scene_buffer()
{
    sceneData.ambientColor = glm::vec4(.1f);
    sceneData.sunlightColor = glm::vec4(1.f);
    sceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);
    mainCamera.updatePosition(stats.frametime, static_cast<float>(ONE_SECOND_IN_MILLISECONDS / EXPECTED_FRAME_RATE));
    sceneData.view = mainCamera.getViewMatrix();
    sceneData.proj = glm::perspective(glm::radians(70.f), static_cast<float>(_windowExtent.width) / static_cast<float>(_windowExtent.height), 10000.f, 0.1f);
    sceneData.proj[1][1] *= -1;
    sceneData.viewproj = sceneData.proj * sceneData.view;

    sceneBuffer = create_buffer(sizeof(SceneData), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, _bufferDeletionQueue.perDrawBuffers);

    const AllocatedBuffer stagingBuffer = create_buffer(sizeof(SceneData), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, _bufferDeletionQueue.perDrawBuffers);
    void* stagingAddress = stagingBuffer.allocation->GetMappedData();
    memcpy(stagingAddress, &sceneData, sizeof(SceneData));

    VkBufferCopy sceneCopy {};
    sceneCopy.dstOffset = 0;
    sceneCopy.srcOffset = 0;
    sceneCopy.size = sizeof(SceneData);

    immediate_submit([&](const VkCommandBuffer cmd) {
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, sceneBuffer.buffer, 1, &sceneCopy);
    });
}

void VulkanEngine::create_material_buffer(PbrMaterial& material)
{
    int materialConstantsSize = sizeof(MaterialConstants);
    materialConstantsBuffer = create_buffer(materialConstantsSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY, _bufferDeletionQueue.perDrawBuffers);

    const AllocatedBuffer stagingBuffer = create_buffer(materialConstantsSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_CPU_ONLY, _bufferDeletionQueue.perDrawBuffers);
    void* stagingAddress = stagingBuffer.allocation->GetMappedData();
    memcpy(stagingAddress, &material.data.constants, materialConstantsSize);

    VkBufferCopy materialCopy {};
    materialCopy.dstOffset = 0;
    materialCopy.srcOffset = 0;
    materialCopy.size = sizeof(MaterialConstants);

    immediate_submit([&](const VkCommandBuffer cmd) {
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, materialConstantsBuffer.buffer, 1, &materialCopy);
    });
}

void VulkanEngine::create_material_texture_array(PbrMaterial& material)
{
    DescriptorWriter writer;
    int materialIndex = 0;

    writer.write_image_array(0, material.data.resources.base.image.imageView, material.data.resources.base.sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, materialIndex);
    writer.write_image_array(0, material.data.resources.emissive.image.imageView, material.data.resources.emissive.sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, materialIndex + 1);
    writer.write_image_array(0, material.data.resources.metallicRoughness.image.imageView, material.data.resources.metallicRoughness.sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, materialIndex + 2);
    writer.write_image_array(0, material.data.resources.normal.image.imageView, material.data.resources.normal.sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, materialIndex + 3);
    writer.write_image_array(0, material.data.resources.occlusion.image.imageView, material.data.resources.occlusion.sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, materialIndex + 4);

    writer.update_set(_device, materialTexturesArrayDescriptorSet);
}

void VulkanEngine::update_scene()
{
    const auto start = std::chrono::system_clock::now();

    indirectBatches.clear();
    indirectBuffers.clear();

    create_vertex_index_buffers();
    create_indirect_commands();
    create_instanced_data();
    create_scene_buffer();

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.scene_update_time = static_cast<float>(elapsed.count()) / 1000.f;
}

void VulkanEngine::cleanup_immediate()
{
    _immediateDeletionQueue.fences.flush();
    _immediateDeletionQueue.commandPools.flush();
}

void VulkanEngine::cleanup_swapchain()
{
    destroy_swapchain();
}

void VulkanEngine::cleanup_descriptors()
{
    _globalDescriptorAllocator.destroy_pools(_device);
    _descriptorDeletionQueue.descriptorSetLayouts.flush();
}

void VulkanEngine::cleanup_pipeline_caches()
{
    write_pipeline_cache(pipelineCacheFile);
    vkDestroyPipelineCache(_device, pipelineCache, nullptr);
}

void VulkanEngine::cleanup_pipelines()
{
    _pipelineDeletionQueue.pipelines.flush();
    _pipelineDeletionQueue.pipelineLayouts.flush();
}

void VulkanEngine::cleanup_samplers()
{
    _samplerDeletionQueue.samplers.flush();
}

void VulkanEngine::cleanup_images()
{
    _imageDeletionQueue.images.flush();
    _imageDeletionQueue.imageViews.flush();
}

void VulkanEngine::cleanup_buffers()
{
    _bufferDeletionQueue.genericBuffers.flush();
    _bufferDeletionQueue.perDrawBuffers.flush();
    _bufferDeletionQueue.tempBuffers.flush();
}

void VulkanEngine::cleanup_imgui() const
{
    vkDestroyDescriptorPool(_device, _imguiDescriptorPool, nullptr);
    ImGui_ImplVulkan_Shutdown();
}

void VulkanEngine::cleanup_misc() const
{
    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkb::destroy_debug_utils_messenger(_instance, _debugMessenger);
}

void VulkanEngine::cleanup_core() const
{
    vmaDestroyAllocator(_allocator);
    vkDestroyDevice(_device, nullptr);
    vkDestroyInstance(_instance, nullptr);
    SDL_DestroyWindow(_window);
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {
        vkDeviceWaitIdle(_device);

        // GLTF scenes cleared by own destructor.
        loadedModels.clear();
        for (FrameData& frame : _frames)
            frame.cleanup(_device);
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

void VulkanEngine::draw()
{
    // Wait until the gpu has finished rendering the frame of this index (become signalled), or until timeout of 1 second (in nanoseconds).
    VK_CHECK(vkWaitForFences(_device, 1, &(get_current_frame()._renderFence), true, 1000000000));
    VK_CHECK(vkResetFences(_device, 1, &(get_current_frame()._renderFence))); // Flip to unsignalled

    get_current_frame()._frameDescriptors.clear_pools(_device);
    get_current_frame()._frameDeletionQueue.bufferDeletion.flush(); // For buffers that are used by cmd buffers. Wait for fence of this frame to be reset before flushing.
    _bufferDeletionQueue.perDrawBuffers.flush();
    update_scene();

    // Request image from the swapchain
    // _swapchainSemaphore signalled only when next image is acquired.
    uint32_t swapchainImageIndex;
    if (vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex) == VK_ERROR_OUT_OF_DATE_KHR) {
        _resize_requested = true;
        return;
    }

    const VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0)); // Reset the command buffer to begin recording again for each frame.
    const VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo)); // Start the command buffer recording

    // Multiply by render scale for dynamic resolution
    // When resizing bigger, don't make swapchain extent go beyond draw image extent
    _drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * _renderScale;
    _drawExtent.width = std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * _renderScale;

    // Transition stock and draw image into transfer layouts
    vkutil::transition_image(cmd, _stockImages["blue"].image,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, _drawImage.image,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy stock image as the initial colour for the draw image (the background)
    vkutil::copy_image_to_image(cmd, _stockImages["blue"].image, _drawImage.image, VkExtent2D {
                                                                                       _stockImages["blue"].imageExtent.width,
                                                                                       _stockImages["blue"].imageExtent.height,
                                                                                   },
        _drawExtent);

    // Transition to color output for drawing coloured triangle
    vkutil::transition_image(cmd, _drawImage.image,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transition_image(cmd, _depthImage.image,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    draw_geometry(cmd);

    // Transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(cmd, _drawImage.image,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy draw image into the swapchain image
    vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

    // Set swapchain image to be attachment optimal to draw it
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Draw imgui into the swapchain image
    draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

    // Set swapchain image layout to presentable layout
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // Finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Prepare the submission to the queue. (Reading semaphore states)
    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);
    const VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    // Submit command buffer to the queue and execute it.
    // _renderFence will block CPU from going to next frame, stays unsignalled until this is done.
    // _swapchainSemaphore gets waited on until it is signalled when the next image is acquired.
    // _renderSemaphore will be signalled by this function when this queue's commands are executed.
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

    // Prepare present.
    // Wait on the _renderSemaphore for queue commands to finish before image is presented.
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex;

    if (vkQueuePresentKHR(_graphicsQueue, &presentInfo) == VK_ERROR_OUT_OF_DATE_KHR)
        _resize_requested = true;

    _frameNumber++;
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
                    _stopRendering = true;
                if (e.window.event == SDL_WINDOWEVENT_RESTORED)
                    _stopRendering = false;
            }
            mainCamera.processSDLEvent(e);
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        // Do not draw if we are minimized
        if (_stopRendering) {
            // Throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Misc options
        SDL_SetRelativeMouseMode(mainCamera.relativeMode);
        if (_resize_requested)
            resize_swapchain();

        // Imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(_window);
        ImGui::NewFrame();
        if (ImGui::Begin("Background")) {
            ImGui::SliderFloat("Render Scale", &_renderScale, 0.3f, 1.f);
            ImGui::Text("[F1] Camera Mode: %s", magic_enum::enum_name(mainCamera.movementMode).data());
            ImGui::Text("[F2] Mouse Mode: %s", (mainCamera.relativeMode ? "RELATIVE" : "NORMAL"));
            if (cvarInstance->intCVars.get("fif")) {
                ImGui::Text("frame-in-flight: %d", cvarInstance->intCVars.get("fif")->value);
            }
            ImGui::End();
        }
        if (ImGui::Begin("Stats")) {
            ImGui::Text("Compile Mode: %s", (bUseValidationLayers ? "DEBUG" : "RELEASE"));
            ImGui::Text("Frame Time:  %fms", stats.frametime);
            ImGui::Text("Draw Time: %fms", stats.mesh_draw_time);
            ImGui::Text("Update Time: %fms", stats.scene_update_time);
            ImGui::Text("Triangles: %i", stats.triangle_count);
            ImGui::Text("Draws: %i", stats.drawcall_count);
            ImGui::End();
        }
        ImGui::Render();

        draw();

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        stats.frametime = static_cast<float>(elapsed.count()) / 1000.f;
    }
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) const
{
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    const VkCommandBuffer cmd = _immCommandBuffer;

    const VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
    function(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    const VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));
    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}
