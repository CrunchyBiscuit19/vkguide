#pragma once
#include <vk_types.h>

namespace vkutil {
bool load_shader_module(const char* filePath,
    VkDevice device,
    VkShaderModule* outShaderModule);

};

struct PipelineCombined {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

class GraphicsPipelineBuilder {
public:
    std::vector<VkPipelineShaderStageCreateInfo> mShaderStages;
    VkPipelineInputAssemblyStateCreateInfo mInputAssembly;
    VkPipelineRasterizationStateCreateInfo mRasterizer;
    VkPipelineColorBlendAttachmentState mColorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo mMultisampling;
    VkPipelineLayout mPipelineLayout;
    VkPipelineDepthStencilStateCreateInfo mDepthStencil;
    VkPipelineRenderingCreateInfo mRenderInfo;
    VkFormat mColorAttachmentformat;
    VkPipelineCache mPipelineCache;

    GraphicsPipelineBuilder();

    void clear();
    VkPipeline build_pipeline(VkDevice device) const;
    void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_polygon_mode(VkPolygonMode mode);
    void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void set_multisampling_none();
    void disable_blending();
    void enable_blending_additive();
    void enable_blending_alphablend();
    void set_color_attachment_format(VkFormat format);
    void set_depth_format(VkFormat format);
    void disable_depthtest();
    void enable_depthtest(bool depthWriteEnable, VkCompareOp op);
};

class ComputePipelineBuilder {
public:
    VkPipelineShaderStageCreateInfo mComputeShaderStageCreateInfo;
    VkPipelineLayout mPipelineLayout;
    VkPipelineCache mPipelineCache;

    ComputePipelineBuilder() = default;

    void set_shader(VkShaderModule computeShader);
    VkPipeline build_pipeline(VkDevice device) const;
};
