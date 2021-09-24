#pragma once
#include "vku.hpp"

namespace vku {
    class DebugCallback {
    public:
        DebugCallback();

        DebugCallback(
            VkInstance instance,
            VkDebugReportFlagsEXT flags =
            VK_DEBUG_REPORT_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_ERROR_BIT_EXT
        );

        void reset();
    private:
        // Report any errors or warnings.
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
            uint64_t object, size_t location, int32_t messageCode,
            const char* pLayerPrefix, const char* pMessage, void* pUserData);

        VkDebugReportCallbackEXT callback_ = VK_NULL_HANDLE;
        VkInstance instance_ = VK_NULL_HANDLE;
    };
}
