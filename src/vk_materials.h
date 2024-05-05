#pragma once

#include "fastgltf/types.hpp"
#include <vk_types.h>

class VulkanEngine;

struct MaterialImage {
    AllocatedImage image;
    VkSampler sampler;
};

struct MaterialConstants {
    glm::vec4 baseFactor;
    glm::vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
};

struct MaterialResources {
    MaterialImage base;
    MaterialImage metallicRoughness;
    MaterialImage normal;
    MaterialImage occlusion;
    MaterialImage emissive;
};

struct PbrData {
    bool doubleSided;
    fastgltf::AlphaMode alphaMode;
    MaterialConstants constants;
    MaterialResources resources;
};

struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

class PbrMaterial {
    VulkanEngine* engine;
    MaterialPipeline pipeline;

public:
    PbrData data;
    int index;

    PbrMaterial(VulkanEngine* engine);

    void create_material();
};