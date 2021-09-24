#pragma once
#include "vku.hpp"

namespace vku {
    /// Factory for instances.
    class InstanceMaker {
    public:
        InstanceMaker();

        /// Set the default layers and extensions.
        InstanceMaker& defaultLayers();

        /// Add a layer. eg. "VK_LAYER_LUNARG_standard_validation"
        InstanceMaker& layer(const char* layer);

        /// Add an extension. eg. VK_EXT_DEBUG_REPORT_EXTENSION_NAME
        InstanceMaker& extension(const char* layer);

        /// Set the name of the application.
        InstanceMaker& applicationName(const char* pApplicationName_);

        /// Set the version of the application.
        InstanceMaker& applicationVersion(uint32_t applicationVersion_);

        /// Set the name of the engine.
        InstanceMaker& engineName(const char* pEngineName_);

        /// Set the version of the engine.
        InstanceMaker& engineVersion(uint32_t engineVersion_);

        /// Set the version of the api.
        InstanceMaker& apiVersion(uint32_t apiVersion_);

        /// Create an instance.
        VkInstance create();
    private:
        std::vector<const char*> layers_;
        std::vector<const char*> instance_extensions_;
        VkApplicationInfo app_info_{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    };
}
