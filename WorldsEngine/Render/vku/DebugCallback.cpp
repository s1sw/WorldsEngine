#include "vku.hpp"
#include "DebugCallback.hpp"

#define UNUSED(x) (void)x
namespace vku {
    DebugCallback::DebugCallback() {
    }

    DebugCallback::DebugCallback(
        VkInstance instance,
        VkDebugReportFlagsEXT flags
    ) : instance_(instance) {
        auto ci = VkDebugReportCallbackCreateInfoEXT{
            VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
            nullptr, flags, &debugCallback, nullptr };

        VkDebugReportCallbackEXT cb{};
        VKCHECK(vkCreateDebugReportCallbackEXT(
            instance_, &ci,
            nullptr, &cb
        ));
        callback_ = cb;
    }

    void DebugCallback::reset() {
        if (callback_) {
            auto vkDestroyDebugReportCallbackEXT =
                (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_,
                    "vkDestroyDebugReportCallbackEXT");
            vkDestroyDebugReportCallbackEXT(instance_, callback_, nullptr);
            callback_ = VK_NULL_HANDLE;
        }
    }

    // Report any errors or warnings.
    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback::debugCallback(
        VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
        uint64_t object, size_t location, int32_t messageCode,
        const char* pLayerPrefix, const char* pMessage, void* pUserData) {
        UNUSED(objectType);
        UNUSED(object);
        UNUSED(location);
        UNUSED(messageCode);
        UNUSED(pLayerPrefix);
        UNUSED(pMessage);
        UNUSED(pUserData);

        if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) == VK_DEBUG_REPORT_ERROR_BIT_EXT) {
            logErr(worlds::WELogCategoryRender, "Vulkan: %s\n", pMessage);
        } else if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) == VK_DEBUG_REPORT_WARNING_BIT_EXT) {
            logWarn(worlds::WELogCategoryRender, "Vulkan: %s\n", pMessage);
        } else {
            logMsg(worlds::WELogCategoryRender, "Vulkan: %s\n", pMessage);
        }
        return VK_FALSE;
    }
}
