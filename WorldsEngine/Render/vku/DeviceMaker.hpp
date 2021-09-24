#pragma once
#include "vku.hpp"

namespace vku {
    /// Factory for devices.
    class DeviceMaker {
    public:
        /// Make queues and a logical device for a certain physical device.
        DeviceMaker();

        /// Set the default layers and extensions.
        DeviceMaker& defaultLayers();

        /// Add a layer. eg. "VK_LAYER_LUNARG_standard_validation"
        DeviceMaker& layer(const char* layer);

        /// Add an extension. eg. VK_EXT_DEBUG_REPORT_EXTENSION_NAME
        DeviceMaker& extension(const char* layer);

        /// Add one or more queues to the device from a certain family.
        DeviceMaker& queue(uint32_t familyIndex, float priority = 0.0f, uint32_t n = 1);

        DeviceMaker& setPNext(void* next);

        DeviceMaker& setFeatures(VkPhysicalDeviceFeatures& features);

        /// Create a new logical device.
        VkDevice create(VkPhysicalDevice physical_device);
    private:
        std::vector<const char*> layers_;
        std::vector<const char*> device_extensions_;
        std::vector<std::vector<float> > queue_priorities_;
        std::vector<VkDeviceQueueCreateInfo> qci_;
        VkPhysicalDeviceFeatures createFeatures;
        void* pNext;
    };
}
