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
    std::unordered_map<std::string, std::shared_ptr<MeshData>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<PbrMaterial>> materials;

    std::vector<std::shared_ptr<Node>> topNodes; // Nodes that dont have a parent, for iterating through the file in tree order
    std::vector<VkSampler> samplers;

    VulkanEngine* creator;

    ~LoadedGLTF() { clearAll(); }

private:
    void clearAll() const;
};

VkFilter extract_filter(fastgltf::Filter filter);
VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(VulkanEngine* engine, std::string_view filePath);
std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image);