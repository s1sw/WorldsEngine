#include "ShaderCache.hpp"
#include <Core/AssetDB.hpp>
#include <Core/Fatal.hpp>
#include <Core/Log.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKPipeline.hpp>
#include <string>

using namespace R2;

namespace worlds
{
    std::unordered_map<AssetID, VK::ShaderModule*> ShaderCache::modules;
    VK::Core* ShaderCache::core;

    void ShaderCache::setDevice(VK::Core* core)
    {
        ShaderCache::core = core;
    }

    VK::ShaderModule& ShaderCache::getModule(AssetID id)
    {
        auto it = modules.find(id);

        if (it != modules.end())
            return *it->second;

        PHYSFS_File* file = worlds::AssetDB::openAssetFileRead(id);

        if (!file)
        {
            std::string msg = "Failed to open shader file" + AssetDB::idToPath(id);
            fatalErr(msg.c_str());
        }

        size_t size = PHYSFS_fileLength(file);
        void* buffer = std::malloc(size);

        size_t readBytes = PHYSFS_readBytes(file, buffer, size);
        if (readBytes != size)
        {
            fatalErr("Failed to read whole shader file");
        }
        PHYSFS_close(file);

        modules.insert({id, new VK::ShaderModule{core->GetHandles(), static_cast<uint32_t*>(buffer), size}});

        std::free(buffer);

        logVrb(WELogCategoryRender, "loading shader %s from disk", AssetDB::idToPath(id).c_str());
        it = modules.find(id);

        return *it->second;
    }

    VK::ShaderModule& ShaderCache::getModule(const char* path)
    {
        return getModule(AssetDB::pathToId(path));
    }

    void ShaderCache::clear()
    {
        for (auto& p : modules)
        {
            delete p.second;
        }

        modules.clear();
    }
}
