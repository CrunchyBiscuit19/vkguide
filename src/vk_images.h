
#pragma once

#include <vulkan/vulkan.h>

namespace vkutil {
void transition_image(VkCommandBuffer cmd, VkImage image, VkPipelineStageFlagBits2 srcStageMask,
    VkAccessFlagBits2 srcAccessMask, VkPipelineStageFlagBits2 dstStageMask,
    VkAccessFlagBits2 dstAccessMask, VkImageLayout currentLayout, VkImageLayout newLayout);
void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
};