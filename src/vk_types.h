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
#include <tuple>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#include <vk_descriptors.h>

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function) { deletors.push_back(function); }
    void flush()
    {
        // Reverse iterate the deletion queue to execute all the functions
        for (auto& deletor : std::ranges::reverse_view(deletors)) {
            deletor(); // call functors
        }
        deletors.clear();
    }
};

template <class T1>
struct DeviceResource
{
    VkDevice device;
    T1 object;
    VkAllocationCallbacks* allocationCallbacks;
};

template <class T1>
struct InstanceResource
{
    VkInstance device;
    T1 object;
    VkAllocationCallbacks* allocationCallbacks;
};

template <class T1>
struct VmaResource
{
    VmaAllocator allocator;
    T1 object;
    VmaAllocation allocation;
};

template <class T0>
class VulkanDeletable {
    std::vector<T0> _resources;

public:
    void pushResource(T0& resource)
    {
        _resources.emplace_back(resource);
    }
    virtual void flush() { }
};

template <>
inline void VulkanDeletable<DeviceResource<VkDescriptorSetLayout>>::flush()
{
    for (const auto& resource : _resources)
        vkDestroyDescriptorSetLayout(resource.device, resource.object, resource.allocationCallbacks);
}
template <>
inline void VulkanDeletable<DeviceResource<VkPipelineLayout>>::flush()
{
    for (const auto& resource : _resources)
        vkDestroyPipelineLayout(resource.device, resource.object, resource.allocationCallbacks);
}
template <>
inline void VulkanDeletable<DeviceResource<VkPipeline>>::flush()
{
    for (const auto& resource : _resources)
        vkDestroyPipeline(resource.device, resource.object, resource.allocationCallbacks);
}
template <>
inline void VulkanDeletable<DeviceResource<VkFence>>::flush()
{
    for (const auto& resource : _resources)
        vkDestroyFence(resource.device, resource.object, resource.allocationCallbacks);
}
template <>
inline void VulkanDeletable<DeviceResource<VkSemaphore>>::flush()
{
    for (const auto& resource : _resources)
        vkDestroySemaphore(resource.device, resource.object, resource.allocationCallbacks);
}
template <>
inline void VulkanDeletable<DeviceResource<VkCommandPool>>::flush()
{
    for (const auto& resource : _resources)
        vkDestroyCommandPool(resource.device, resource.object, resource.allocationCallbacks);
}
template<>
inline void VulkanDeletable<DeviceResource<VkSwapchainKHR>>::flush()
{
    for (const auto& resource : _resources)
        vkDestroySwapchainKHR(resource.device, resource.object, resource.allocationCallbacks);
}
template<>
inline void VulkanDeletable<DeviceResource<VkImageView>>::flush()
{
    for (const auto& resource : _resources)
        vkDestroyImageView(resource.device, resource.object, resource.allocationCallbacks);
}

template <>
inline void VulkanDeletable<VmaResource<VkImage>>::flush()
{
    for (const auto& resource : _resources)
        vmaDestroyImage(resource.allocator, resource.object, resource.allocation);
}
template <>
inline void VulkanDeletable<VmaResource<VkBuffer>>::flush()
{
    for (const auto& resource : _resources)
        vmaDestroyBuffer(resource.allocator, resource.object, resource.allocation);
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

    VulkanDeletable<DeviceResource<VkFence>> fenceDeletion;
    VulkanDeletable<DeviceResource<VkSemaphore>> semaphoreDeletion;
    VulkanDeletable<DeviceResource<VkCommandPool>> commandPoolDeletion;

    void cleanup(VkDevice device)
    {
        fenceDeletion.flush();
        semaphoreDeletion.flush();
        commandPoolDeletion.flush();
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

struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct ComputePushConstants {
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

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

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
    std::string name;
    std::vector<GeoSurface> surfaces; // Mesh primitives, one material per primitve
    GPUMeshBuffers meshBuffers;
};

struct ComputeEffect {
    const char* name;
    VkPipeline pipeline;
    VkPipelineLayout layout;
    ComputePushConstants data;
};

struct DrawContext;

// Base class for a renderable dynamic object
class IRenderable {
    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// Implementation of a drawable scene node.
// The scene node can hold children and will also keep a transform to propagate to them (ie all children nodes also get transformed).
struct Node : public IRenderable {
    // Parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform; // proj * view * localTransform

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (const auto& c : children)
            c->refreshTransform(worldTransform);
    }
    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx)
    {
        for (const auto& c : children)
            c->Draw(topMatrix, ctx);
    }
};

#define VK_CHECK(x)                                                          \
    do {                                                                     \
        VkResult err = x;                                                    \
        if (err) {                                                           \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                         \
        }                                                                    \
    } while (0)