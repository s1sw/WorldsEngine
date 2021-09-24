#pragma once
#include <vulkan/vulkan_core.h>
#include "Core/Fatal.hpp"
#include "StringConversions.hpp"
#define VKCHECK(expr) vku::checkVkResult(expr, __FILE__, __LINE__)

namespace vku {
    inline VkResult checkVkResult(VkResult result, const char* file, int line) {
        switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        case VK_ERROR_DEVICE_LOST:
            fatalErrInternal(vku::toString(result), file, line);
            break;
        default: return result;
        }

        return result;
    }
}
