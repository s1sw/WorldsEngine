#include "vku.hpp"

namespace vku {
    /// Make queues and a logical device for a certain physical device.
    DeviceMaker::DeviceMaker() : pNext(nullptr) {
    }

    /// Set the default layers and extensions.
    DeviceMaker& DeviceMaker::defaultLayers() {
        layers_.push_back("VK_LAYER_LUNARG_standard_validation");
        device_extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        return *this;
    }

    /// Add a layer. eg. "VK_LAYER_LUNARG_standard_validation"
    DeviceMaker& DeviceMaker::layer(const char* layer) {
        layers_.push_back(layer);
        return *this;
    }

    /// Add an extension. eg. VK_EXT_DEBUG_REPORT_EXTENSION_NAME
    DeviceMaker& DeviceMaker::extension(const char* layer) {
        device_extensions_.push_back(layer);
        return *this;
    }

    /// Add one or more queues to the device from a certain family.
    DeviceMaker& DeviceMaker::queue(uint32_t familyIndex, float priority, uint32_t n) {
        queue_priorities_.emplace_back(n, priority);

        qci_.emplace_back(
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            nullptr,
            VkDeviceQueueCreateFlags{},
            familyIndex, n,
            queue_priorities_.back().data()
        );

        return *this;
    }

    DeviceMaker& DeviceMaker::setPNext(void* next) {
        pNext = next;
        return *this;
    }

    DeviceMaker& DeviceMaker::setFeatures(VkPhysicalDeviceFeatures& features) {
        createFeatures = features;
        return *this;
    }

    /// Create a new logical device.
    VkDevice DeviceMaker::create(VkPhysicalDevice physical_device) {
        VkDeviceCreateInfo dci{
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr,
            {}, (uint32_t)qci_.size(), qci_.data(),
            (uint32_t)layers_.size(), layers_.data(),
            (uint32_t)device_extensions_.size(), device_extensions_.data(), &createFeatures };
        dci.pNext = pNext;

        VkDevice device;
        VKCHECK(vkCreateDevice(physical_device, &dci, nullptr, &device));

        return device;
    }
}
