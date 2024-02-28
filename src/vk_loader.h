#pragma once

#include <filesystem>
#include <unordered_map>

#include <vk_types.h>

// forward declaration
class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath);