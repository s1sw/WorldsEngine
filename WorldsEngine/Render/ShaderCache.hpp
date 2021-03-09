#pragma once
#include "vku/vku.hpp"
#include <unordered_map>

namespace worlds {
    // Caches loaded shaders in memory to avoid reloading them from disk.
    class ShaderCache {
    public:
        static vk::ShaderModule getModule(vk::Device& dev, AssetID id);
        static void clear() { modules.clear(); }
    private:
        static std::unordered_map<AssetID, vk::ShaderModule> modules;
    };
}