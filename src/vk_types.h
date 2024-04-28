// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <array>
#include <deque>
#include <fmt/core.h>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/fwd.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#include <vk_descriptors.h>

struct MaterialInstance;
struct MeshData;

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

struct FrameData {

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;

    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;

    DescriptorAllocatorGrowable _frameDescriptors;

    struct FrameDeletionQueue {
        DeletionQueue<VkFence> fenceDeletion;
        DeletionQueue<VkSemaphore> semaphoreDeletion;
        DeletionQueue<VkCommandPool> commandPoolDeletion;
    } _frameDeletionQueue;

    void cleanup(VkDevice device)
    {
        _frameDeletionQueue.fenceDeletion.flush();
        _frameDeletionQueue.semaphoreDeletion.flush();
        _frameDeletionQueue.commandPoolDeletion.flush();
        _frameDescriptors.destroy_pools(device);
    }
};

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct MeshBuffers {
    AllocatedBuffer indexBuffer;
    int indexCount;
    AllocatedBuffer vertexBuffer;
    int vertexCount;
    VkDeviceAddress vertexBufferAddress;
};

struct SSBOAddresses {
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress instanceBuffer;
    VkDeviceAddress sceneBuffer;
};

struct SceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

struct InstanceData {
    glm::mat4 translation;
    glm::mat4 rotation;
    glm::mat4 scale;
};

struct IndirectBatch {
    MeshData* mesh;
    MaterialInstance* material;
    VkDrawIndexedIndirectCommand indirectCommand;
};

#define VK_CHECK(x)                                                          \
    do {                                                                     \
        VkResult err = x;                                                    \
        if (err) {                                                           \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                         \
        }                                                                    \
    } while (0)