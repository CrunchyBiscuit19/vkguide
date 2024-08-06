#include <fstream>
#include <vk_initializers.h>
#include <vk_pipelines.h>

bool vkutil::load_shader_module(const char* filePath,
    VkDevice device,
    VkShaderModule* outShaderModule)
{
    // Open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        return false;

    // Find size of the file by looking at the location of the cursor
    const size_t fileSize = file.tellg();

    // SPIRV expects the buffer to be on uint32
    // Reserve vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    file.seekg(0); // File cursor at start
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize)); // Load whole file into buffer
    file.close();

    // Create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.codeSize = buffer.size() * sizeof(uint32_t); // codeSize has to be in bytes
    createInfo.pCode = buffer.data();

    if (vkCreateShaderModule(device, &createInfo, nullptr, outShaderModule) != VK_SUCCESS)
        return false;

    return true;
}

GraphicsPipelineBuilder::GraphicsPipelineBuilder()
{
    clear();
}

void GraphicsPipelineBuilder::clear()
{
    // Clear all of the structs we need back to 0 with their correct sType
    mInputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    mRasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    mColorBlendAttachment = {};
    mMultisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    mPipelineLayout = {};
    mDepthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    mRenderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    mShaderStages.clear();
}

VkPipeline GraphicsPipelineBuilder::build_pipeline(VkDevice device) const
{
    // Make viewport state from our stored viewport and scissor.
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    constexpr VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;

    // Setup dummy color blending, no blend.
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &mColorBlendAttachment;

    // Completely clear VertexInputStateCreateInfo, as we have no need for it.
    constexpr VkPipelineVertexInputStateCreateInfo _vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    // Use all the info structs to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    // connect the renderInfo to the pNext extension mechanism
    pipelineInfo.pNext = &mRenderInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(mShaderStages.size());
    pipelineInfo.pStages = mShaderStages.data();
    pipelineInfo.pVertexInputState = &_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &mInputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &mRasterizer;
    pipelineInfo.pMultisampleState = &mMultisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &mDepthStencil;
    pipelineInfo.layout = mPipelineLayout;
    pipelineInfo.pDynamicState = &dynamicInfo;

    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, mPipelineCache, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        fmt::println("Failed to create pipeline");
        return VK_NULL_HANDLE;
    }
    return newPipeline;
}

void GraphicsPipelineBuilder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    mShaderStages.clear();
    mShaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
    mShaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void GraphicsPipelineBuilder::set_input_topology(VkPrimitiveTopology topology)
{
    mInputAssembly.topology = topology;
    mInputAssembly.primitiveRestartEnable = VK_FALSE; // For strips
}

void GraphicsPipelineBuilder::set_polygon_mode(VkPolygonMode mode)
{
    mRasterizer.polygonMode = mode;
    mRasterizer.lineWidth = 1.f;
}

void GraphicsPipelineBuilder::set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    mRasterizer.cullMode = cullMode;
    mRasterizer.frontFace = frontFace;
}

void GraphicsPipelineBuilder::set_multisampling_none()
{
    mMultisampling.sampleShadingEnable = VK_FALSE;
    // Multisampling defaulted to no multisampling (1 sample per pixel)
    mMultisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    mMultisampling.minSampleShading = 1.0f;
    mMultisampling.pSampleMask = nullptr;
    // No alpha to coverage either
    mMultisampling.alphaToCoverageEnable = VK_FALSE;
    mMultisampling.alphaToOneEnable = VK_FALSE;
}

void GraphicsPipelineBuilder::disable_blending()
{
    // default write mask
    mColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // no blending
    mColorBlendAttachment.blendEnable = VK_FALSE;
}

void GraphicsPipelineBuilder::enable_blending_additive()
{
    mColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    mColorBlendAttachment.blendEnable = VK_TRUE;
    mColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    mColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    mColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    mColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    mColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    mColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void GraphicsPipelineBuilder::enable_blending_alphablend()
{
    mColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    mColorBlendAttachment.blendEnable = VK_TRUE;
    mColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    mColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    mColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    mColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    mColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    mColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void GraphicsPipelineBuilder::set_color_attachment_format(VkFormat format)
{
    mColorAttachmentformat = format;
    mRenderInfo.colorAttachmentCount = 1;
    mRenderInfo.pColorAttachmentFormats = &mColorAttachmentformat;
}

void GraphicsPipelineBuilder::set_depth_format(VkFormat format)
{
    mRenderInfo.depthAttachmentFormat = format;
}

void GraphicsPipelineBuilder::disable_depthtest()
{
    mDepthStencil.depthTestEnable = VK_FALSE;
    mDepthStencil.depthWriteEnable = VK_FALSE;
    mDepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    mDepthStencil.depthBoundsTestEnable = VK_FALSE;
    mDepthStencil.stencilTestEnable = VK_FALSE;
    mDepthStencil.front = {};
    mDepthStencil.back = {};
    mDepthStencil.minDepthBounds = 0.f;
    mDepthStencil.maxDepthBounds = 1.f;
}

void GraphicsPipelineBuilder::enable_depthtest(bool depthWriteEnable, VkCompareOp op)
{
    mDepthStencil.depthTestEnable = VK_TRUE;
    mDepthStencil.depthWriteEnable = depthWriteEnable;
    mDepthStencil.depthCompareOp = op;
    mDepthStencil.depthBoundsTestEnable = VK_FALSE;
    mDepthStencil.stencilTestEnable = VK_FALSE;
    mDepthStencil.front = {};
    mDepthStencil.back = {};
    mDepthStencil.minDepthBounds = 0.f;
    mDepthStencil.maxDepthBounds = 1.f;
}

void ComputePipelineBuilder::set_shader(VkShaderModule computeShader)
{
    mComputeShaderStageCreateInfo = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, computeShader);
}

VkPipeline ComputePipelineBuilder::build_pipeline(VkDevice device) const
{
    VkComputePipelineCreateInfo computePipelineInfo {};
    computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineInfo.layout = mPipelineLayout;
    computePipelineInfo.stage = mComputeShaderStageCreateInfo;

    VkPipeline newComputePipeline;
    if (vkCreateComputePipelines(device, mPipelineCache, 1, &computePipelineInfo, nullptr, &newComputePipeline) != VK_SUCCESS) {
        fmt::println("Failed to create pipeline");
        return VK_NULL_HANDLE;
    }
    return newComputePipeline;
}