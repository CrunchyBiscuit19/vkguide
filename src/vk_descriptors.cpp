#include "vk_types.h"
#include <vk_descriptors.h>

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type, uint32_t count)
{
    VkDescriptorSetLayoutBinding newbind {};
    newbind.binding = binding;
    newbind.descriptorType = type;
    newbind.descriptorCount = count;

    mBindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
    mBindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, bool useBindless)
{
    for (auto& b : mBindings) {
        b.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo info { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.pNext = nullptr;
    info.pBindings = mBindings.data();
    info.bindingCount = static_cast<uint32_t>(mBindings.size());
    info.flags = 0;

    VkDescriptorBindingFlags bindlessFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
    extended_info.pNext = nullptr;
    extended_info.bindingCount = static_cast<uint32_t>(mBindings.size());
    extended_info.pBindingFlags = &bindlessFlags;
    if (useBindless) {
        info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
        info.pNext = &extended_info;
    }

    VkDescriptorSetLayout setLayout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &setLayout));

    return setLayout;
}

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
    mRatios.clear();
    for (auto r : poolRatios)
        mRatios.push_back(r);

    const VkDescriptorPool newPool = create_pool(device, maxSets, poolRatios);
    mReadyPools.push_back(newPool);

    mSetsPerPool *= 1.5; // Grow it next allocation
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device)
{
    // Reset all, which makes all pools become ready
    for (const auto p : mReadyPools)
        vkResetDescriptorPool(device, p, 0);

    for (const auto p : mFullPools) {
        vkResetDescriptorPool(device, p, 0);
        mReadyPools.push_back(p);
    }

    mFullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device)
{
    for (const auto p : mReadyPools)
        vkDestroyDescriptorPool(device, p, nullptr);
    mReadyPools.clear();

    for (const auto p : mFullPools)
        vkDestroyDescriptorPool(device, p, nullptr);
    mFullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, bool useBindless, uint32_t maxBindings)
{
    // Get or create a pool to allocate from
    VkDescriptorPool poolToUse = get_pool(device);

    VkDescriptorSetAllocateInfo allocInfo { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT countInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT };
    uint32_t maxBinding = maxBindings - 1;
    countInfo.descriptorSetCount = 1;
    countInfo.pDescriptorCounts = &maxBinding;
    if (useBindless) 
        allocInfo.pNext = &countInfo;

    VkDescriptorSet ds;
    const VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

    // First time fail, ready pool is already maxed out
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        mFullPools.push_back(poolToUse);

        // Second time fail, it's completely broken (can't it also be another maxed ready pool?)
        poolToUse = get_pool(device);
        allocInfo.descriptorPool = poolToUse;

        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    }

    mReadyPools.push_back(poolToUse);
    return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device)
{
    VkDescriptorPool newPool;

    // Check if available pools to use, else create new pool
    if (!mReadyPools.empty()) {
        newPool = mReadyPools.back();
        mReadyPools.pop_back();
    } else {
        newPool = create_pool(device, mSetsPerPool, mRatios);
        mSetsPerPool *= 1.5;
        if (mSetsPerPool > 4092) // Why are we resizing it bigger?
            mSetsPerPool = 4092;
    }
    return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const PoolSizeRatio ratio : poolRatios)
        poolSizes.push_back(VkDescriptorPoolSize {
            .type = ratio.type,
            .descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount) });

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    pool_info.maxSets = setCount;
    pool_info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    pool_info.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool);
    return newPool;
}

void DescriptorWriter::write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
{
    const VkDescriptorImageInfo& info = mImageInfos.emplace_back(VkDescriptorImageInfo {
        .sampler = sampler,
        .imageView = image,
        .imageLayout = layout });

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.descriptorCount = 1;
    // write.dstArrayElement = ;
    write.descriptorType = type;
    write.dstSet = VK_NULL_HANDLE; // left empty for now until we need to write it
    write.dstBinding = binding;
    write.pImageInfo = &info;

    mWrites.push_back(write);
}

void DescriptorWriter::write_image_array(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type, uint32_t arrayIndex)
{
    const VkDescriptorImageInfo& info = mImageInfos.emplace_back(VkDescriptorImageInfo {
        .sampler = sampler,
        .imageView = image,
        .imageLayout = layout });

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.descriptorCount = 1;
    write.dstArrayElement = arrayIndex;
    write.descriptorType = type;
    write.dstSet = VK_NULL_HANDLE;
    write.dstBinding = binding;
    write.pImageInfo = &info;

    mWrites.push_back(write);
}

void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
{
    const VkDescriptorBufferInfo& info = mBufferInfos.emplace_back(VkDescriptorBufferInfo {
        .buffer = buffer,
        .offset = offset,
        .range = size });

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; // left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;

    mWrites.push_back(write);
}

void DescriptorWriter::clear()
{
    mImageInfos.clear();
    mBufferInfos.clear();
    mWrites.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : mWrites)
        write.dstSet = set;
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(mWrites.size()), mWrites.data(), 0, nullptr);
}
