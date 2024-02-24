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

PipelineBuilder::PipelineBuilder()
{
    clear();
}

void PipelineBuilder::clear()
{
    // Clear all of the structs we need back to 0 with their correct sType
    _inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    _rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    _colorBlendAttachment = {};
    _multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    _pipelineLayout = {};
    _depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    _renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    _shaderStages.clear();
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device) const
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
    colorBlending.pAttachments = &_colorBlendAttachment;

    // Completely clear VertexInputStateCreateInfo, as we have no need for it.
    constexpr VkPipelineVertexInputStateCreateInfo _vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    // Use all the info structs to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    // connect the renderInfo to the pNext extension mechanism
    pipelineInfo.pNext = &_renderInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(_shaderStages.size());
    pipelineInfo.pStages = _shaderStages.data();
    pipelineInfo.pVertexInputState = &_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &_inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &_rasterizer;
    pipelineInfo.pMultisampleState = &_multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &_depthStencil;
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.pDynamicState = &dynamicInfo;

    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        fmt::println("Failed to create pipeline");
        return VK_NULL_HANDLE;
    } 
	return newPipeline;
}

void PipelineBuilder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
        _shaderStages.clear();
        _shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
        _shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void PipelineBuilder::set_input_topology(VkPrimitiveTopology topology)
{
        _inputAssembly.topology = topology;
        _inputAssembly.primitiveRestartEnable = VK_FALSE; // For strips
}

void PipelineBuilder::set_polygon_mode(VkPolygonMode mode)
{
        _rasterizer.polygonMode = mode;
        _rasterizer.lineWidth = 1.f;
}

void PipelineBuilder::set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
        _rasterizer.cullMode = cullMode;
        _rasterizer.frontFace = frontFace;
}

void PipelineBuilder::set_multisampling_none()
{
        _multisampling.sampleShadingEnable = VK_FALSE;
        // Multisampling defaulted to no multisampling (1 sample per pixel)
        _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        _multisampling.minSampleShading = 1.0f;
        _multisampling.pSampleMask = nullptr;
        // No alpha to coverage either
        _multisampling.alphaToCoverageEnable = VK_FALSE;
        _multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::disable_blending()
{
        // default write mask
        _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        // no blending
        _colorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::enable_blending_additive()
{
        _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        _colorBlendAttachment.blendEnable = VK_TRUE;
        _colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        _colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        _colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        _colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        _colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        _colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::enable_blending_alphablend()
{
        _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        _colorBlendAttachment.blendEnable = VK_TRUE;
        _colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        _colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        _colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        _colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        _colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        _colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::set_color_attachment_format(VkFormat format)
{
        _colorAttachmentformat = format;
        _renderInfo.colorAttachmentCount = 1;
        _renderInfo.pColorAttachmentFormats = &_colorAttachmentformat;
}

void PipelineBuilder::set_depth_format(VkFormat format)
{
        _renderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::disable_depthtest()
{
        _depthStencil.depthTestEnable = VK_FALSE;
        _depthStencil.depthWriteEnable = VK_FALSE;
        _depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
        _depthStencil.depthBoundsTestEnable = VK_FALSE;
        _depthStencil.stencilTestEnable = VK_FALSE;
        _depthStencil.front = {};
        _depthStencil.back = {};
        _depthStencil.minDepthBounds = 0.f;
        _depthStencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::enable_depthtest(bool depthWriteEnable, VkCompareOp op)
{
        _depthStencil.depthTestEnable = VK_TRUE;
        _depthStencil.depthWriteEnable = depthWriteEnable;
        _depthStencil.depthCompareOp = op;
        _depthStencil.depthBoundsTestEnable = VK_FALSE;
        _depthStencil.stencilTestEnable = VK_FALSE;
        _depthStencil.front = {};
        _depthStencil.back = {};
        _depthStencil.minDepthBounds = 0.f;
        _depthStencil.maxDepthBounds = 1.f;
}

