#pragma once

#include <vk_types.h>

struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

enum class MaterialPass : uint8_t {
    MainColor,
    Transparent,
    Other
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

struct GLTFMaterial {
    MaterialInstance data;
};

class VulkanEngine;

struct GLTFMetallicRough {
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    VkDescriptorSetLayout materialLayout;

    struct MaterialConstants {
        glm::vec4 colorFactors;
        glm::vec4 metalRoughFactors;
        // Padding, we need it anyway for uniform buffers
        glm::vec4 extra[14];
    };

    struct MaterialResources {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    DescriptorWriter writer;

    struct MaterialDeletionQueue {
        DeletionQueue<VkDescriptorSetLayout> descriptorSetLayoutDeletion;
        DeletionQueue<VkPipelineLayout> pipelineLayoutDeletion;
        DeletionQueue<VkPipeline> pipelineDeletion;
    } _materialDeletionQueue;

    void build_pipelines(VulkanEngine* engine);
    void cleanup_resources(VkDevice device);

    MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};