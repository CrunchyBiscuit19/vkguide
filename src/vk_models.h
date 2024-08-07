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

    std::vector<std::shared_ptr<Node>> mNodes;
    std::vector<std::shared_ptr<Node>> mTopNodes;

    std::vector<std::shared_ptr<MeshData>> mMeshes;

    std::vector<AllocatedImage> mImages;
    std::vector<VkSampler> mSamplers;

    std::vector<std::shared_ptr<PbrMaterial>> mMaterials;

    VulkanEngine* mEngine;

    GLTFModel(VulkanEngine* engine, fastgltf::Asset& asset, std::filesystem::path modelPath);
    ~GLTFModel();

private:
    static VkFilter extract_filter(fastgltf::Filter filter);
    static VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
    static std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image);

    void cleanup() const;
};

std::optional<std::shared_ptr<GLTFModel>> load_gltf_model(VulkanEngine* engine, std::filesystem::path filePath);