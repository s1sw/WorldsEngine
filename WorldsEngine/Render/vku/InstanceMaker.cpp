#include "vku.hpp"

namespace vku {
    InstanceMaker::InstanceMaker() {}

    InstanceMaker& InstanceMaker::defaultLayers() {
        layers_.push_back("VK_LAYER_LUNARG_standard_validation");
        instance_extensions_.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#ifdef VKU_SURFACE
        instance_extensions_.push_back(VKU_SURFACE);
#endif
        instance_extensions_.push_back("VK_KHR_surface");
#if defined( __APPLE__ ) && defined(VK_EXT_METAL_SURFACE_EXTENSION_NAME)
        instance_extensions_.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#endif //__APPLE__
        return *this;
    }

    InstanceMaker& InstanceMaker::layer(const char* layer) {
        layers_.push_back(layer);
        return *this;
    }

    InstanceMaker& InstanceMaker::extension(const char* layer) {
        instance_extensions_.push_back(layer);
        return *this;
    }

    InstanceMaker& InstanceMaker::applicationName(const char* pApplicationName_) {
        app_info_.pApplicationName = pApplicationName_;
        return *this;
    }

    InstanceMaker& InstanceMaker::applicationVersion(uint32_t applicationVersion_) {
        app_info_.applicationVersion = applicationVersion_;
        return *this;
    }

    InstanceMaker& InstanceMaker::engineName(const char* pEngineName_) {
        app_info_.pEngineName = pEngineName_;
        return *this;
    }

    InstanceMaker& InstanceMaker::engineVersion(uint32_t engineVersion_) {
        app_info_.engineVersion = engineVersion_;
        return *this;
    }

    InstanceMaker& InstanceMaker::apiVersion(uint32_t apiVersion_) {
        app_info_.apiVersion = apiVersion_;
        return *this;
    }

    VkInstance InstanceMaker::create() {
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &app_info_;
        createInfo.enabledLayerCount = layers_.size();
        createInfo.enabledExtensionCount = instance_extensions_.size();

        createInfo.ppEnabledLayerNames = layers_.data();
        createInfo.ppEnabledExtensionNames = instance_extensions_.data();

        VkInstance instance;
        vkCreateInstance(&createInfo, nullptr, &instance);

        return instance;
    }
}
