// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <array>
#include <deque>
#include <fmt/core.h>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>
#include <fastgltf/types.hpp>

#include <vk_descriptors.h>

struct PbrData;
struct PbrMaterial;
struct MeshData;
struct MeshNode;

template <class T0>
struct VulkanResource {
    VkDevice device;
    T0 object;
    VkAllocationCallbacks* allocationCallbacks;
    VmaAllocator allocator;
    VmaAllocation allocation;

    VulkanResource(VkDevice device, T0 object, VkAllocationCallbacks* allocationCallback, VmaAllocator allocator = nullptr, VmaAllocation allocation = nullptr)
        : device(device)
        , object(object)
        , allocationCallbacks(allocationCallback)
        , allocator(allocator)
        , allocation(allocation)
    {
    }
    virtual void destroy()
    {
    }
};

template <class T0>
class DeletionQueue {
    std::vector<VulkanResource<T0>> _resources;

public:
    void push_resource(VkDevice device, T0 object, VkAllocationCallbacks* allocationCallback, VmaAllocator allocator = nullptr, VmaAllocation allocation = nullptr)
    {
        _resources.emplace_back(device, object, allocationCallback, allocator, allocation);
    }
    void flush()
    {
        for (auto& resource : _resources)
            resource.destroy();
        _resources.clear();
    }
};

template <>
inline void VulkanResource<VkDescriptorSetLayout>::destroy()
{
    vkDestroyDescriptorSetLayout(device, object, allocationCallbacks);
}
template <>
inline void VulkanResource<VkPipelineLayout>::destroy()
{
    vkDestroyPipelineLayout(device, object, allocationCallbacks);
}
template <>
inline void VulkanResource<VkPipeline>::destroy()
{

    vkDestroyPipeline(device, object, allocationCallbacks);
}
template <>
inline void VulkanResource<VkFence>::destroy()
{

    vkDestroyFence(device, object, allocationCallbacks);
}
template <>
inline void VulkanResource<VkSemaphore>::destroy()
{

    vkDestroySemaphore(device, object, allocationCallbacks);
}
template <>
inline void VulkanResource<VkCommandPool>::destroy()
{

    vkDestroyCommandPool(device, object, allocationCallbacks);
}
template <>
inline void VulkanResource<VkSwapchainKHR>::destroy()
{

    vkDestroySwapchainKHR(device, object, allocationCallbacks);
}
template <>
inline void VulkanResource<VkImageView>::destroy()
{

    vkDestroyImageView(device, object, allocationCallbacks);
}
template <>
inline void VulkanResource<VkSampler>::destroy()
{
    vkDestroySampler(device, object, allocationCallbacks);
}
template <>
inline void VulkanResource<VkImage>::destroy()
{
    vmaDestroyImage(allocator, object, allocation);
}
template <>
inline void VulkanResource<VkBuffer>::destroy()
{
    vmaDestroyBuffer(allocator, object, allocation);
}

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct SSBOAddresses {
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress instanceBuffer;
    VkDeviceAddress sceneBuffer;
    VkDeviceAddress materialBuffer;
    VkDeviceAddress transformBuffer;
    uint32_t materialIndex;
    uint32_t nodeIndex;
};

struct SceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

struct InstanceData {
    glm::mat4 translation;
    glm::mat4 rotation;
    glm::mat4 scale;
};

struct DescriptorCombined {
    VkDescriptorSet set;
    VkDescriptorSetLayout layout;
};

struct PipelineOptions {
    bool doubleSided;
    fastgltf::AlphaMode alphaMode;

    inline bool operator==(const PipelineOptions& other) const
    {
        return (doubleSided == other.doubleSided && alphaMode == other.alphaMode);
    }
};

template <>
struct std::hash<PipelineOptions> {
    // Compute individual hash values for strings
    // Combine them using XOR and bit shifting
    inline std::size_t operator()(const PipelineOptions& k) const
    {
        return ((std::hash<bool>()(k.doubleSided) ^ (std::hash<fastgltf::AlphaMode>()(k.alphaMode) << 1)) >> 1);
    }
};

struct BufferCopyBatch {
    VkBuffer srcBuffer;
    VkBuffer dstBuffer;
    std::vector<VkBufferCopy> bufferCopies;
};

struct IndirectBatchGroup {
    MeshNode* node;
    PbrMaterial* mat;

    inline bool operator==(const IndirectBatchGroup& other) const
    {
        return (mat == other.mat && node == other.node);
    }

    inline bool operator<(const IndirectBatchGroup& other) const
    {
        if (node != other.node) {
            return (node < other.node);
        }
        return mat < other.mat;
    }
};

template <>
struct std::hash<IndirectBatchGroup> {
    // Compute individual hash values for strings
    // Combine them using XOR and bit shifting
    inline std::size_t operator()(const IndirectBatchGroup& k) const
    {
        return ((std::hash<PbrMaterial*>()(k.mat) ^ (std::hash<MeshNode*>()(k.node) << 1)) >> 1);
    }
};

struct IndirectBatchData {
    std::vector<VkDrawIndexedIndirectCommand> commands;
};

#define VK_CHECK(x)                                                          \
    do {                                                                     \
        VkResult err = x;                                                    \
        if (err) {                                                           \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                         \
        }                                                                    \
    } while (0)