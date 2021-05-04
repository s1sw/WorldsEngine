#pragma once
#include "vku/vku.hpp"
#include <unordered_map>

namespace worlds {
    // Caches loaded shaders in memory to avoid reloading them from disk.
    class ShaderCache {
    public:
        static void setDevice(vk::Device dev) { device = dev; }
        static vk::ShaderModule getModule(vk::Device& dev, AssetID id);
        static void clear();
    private:
        static vk::Device device;
        static std::unordered_map<AssetID, vk::ShaderModule> modules;
    };
}
