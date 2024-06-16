#pragma once

#include <vk_types.h>
#include <vk_meshes.h>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include <filesystem>
#include <unordered_map>


// forward declaration
class VulkanEngine;

struct LoadedGLTF {
    // Storage for all the data on a given glTF file
    std::unordered_map<std::string, std::shared_ptr<MeshData>> mMeshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> mNodes;
    std::unordered_map<std::string, AllocatedImage> mImages;
    std::unordered_map<std::string, std::shared_ptr<PbrMaterial>> mMaterials;
    std::vector<std::shared_ptr<Node>> mTopNodes; // Nodes that dont have a parent, for iterating through the file in tree order
    std::vector<VkSampler> mSamplers;

    VulkanEngine* mEngine;

    ~LoadedGLTF() { clearAll(); }

private:
    void clearAll() const;
};

VkFilter extract_filter(fastgltf::Filter filter);
VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(VulkanEngine* engine, std::string_view filePath);
std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image);