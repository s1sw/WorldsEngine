#pragma once
#include "vku/vku.hpp"
#include <unordered_map>

namespace worlds {
    // Caches loaded shaders in memory to avoid reloading them from disk.
    class ShaderCache {
    public:
        static void setDevice(VkDevice dev) { device = dev; }
        static VkShaderModule getModule(VkDevice& dev, AssetID id);
        static void clear();
    private:
        static VkDevice device;
        static std::unordered_map<AssetID, VkShaderModule> modules;
    };
}
