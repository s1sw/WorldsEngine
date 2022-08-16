#pragma once
#include <unordered_map>

namespace R2::VK
{
    class Core;
    class ShaderModule;
}

namespace worlds
{
    typedef uint32_t AssetID;
    // Caches loaded shaders in memory to avoid reloading them from disk.
    class ShaderCache
    {
    public:
        static void setDevice(R2::VK::Core* core);
        static R2::VK::ShaderModule& getModule(AssetID id);
        static void clear();

    private:
        static R2::VK::Core* core;
        static std::unordered_map<AssetID, R2::VK::ShaderModule*> modules;
    };
}
