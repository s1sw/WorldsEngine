#pragma once
#include <vulkan/vulkan_core.h>

namespace vku {
    const char* toString(VkPhysicalDeviceType type);
    const char* toString(VkMemoryPropertyFlags flags);
    const char* toString(VkResult result);
}
