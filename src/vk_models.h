#pragma once

#include <vk_meshes.h>
#include <vk_types.h>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include <filesystem>
#include <unordered_map>

class VulkanEngine;

struct ModelBuffers {
    AllocatedBuffer index;
    AllocatedBuffer vertex;
};

class GLTFModel {
public:
    std::string mName;

    ModelBuffers mModelBuffers;

    std::unordered_map<std::string, std::shared_ptr<Node>> mNodes;
    std::vector<std::shared_ptr<Node>> mTopNodes;

    std::unordered_map<std::string, std::shared_ptr<MeshData>> mMeshes;

    std::unordered_map<std::string, AllocatedImage> mImages;
    std::vector<VkSampler> mSamplers;

    std::unordered_map<std::string, std::shared_ptr<PbrMaterial>> mMaterials;

    VulkanEngine* mEngine;

    GLTFModel(VulkanEngine* engine, fastgltf::Asset& asset);
    ~GLTFModel();

private:
    VkFilter extract_filter(fastgltf::Filter filter);
    VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
    std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image);

    void cleanup() const;
};

std::optional<std::shared_ptr<GLTFModel>> load_gltf_model(VulkanEngine* engine, std::filesystem::path filePath);