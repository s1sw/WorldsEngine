#include "ShaderCache.hpp"

namespace worlds {
    std::unordered_map<AssetID, vk::ShaderModule> ShaderCache::modules;
    vk::Device ShaderCache::device;
    vk::ShaderModule ShaderCache::getModule(vk::Device& dev, AssetID id) {
        auto it = modules.find(id);

        if (it != modules.end())
            return it->second;

        PHYSFS_File* file = worlds::AssetDB::openAssetFileRead(id);

        if (!file) {
            logErr(WELogCategoryRender, "Failed to open shader file %s", AssetDB::idToPath(id).c_str());
        }

        size_t size = PHYSFS_fileLength(file);
        void* buffer = std::malloc(size);

        size_t readBytes = PHYSFS_readBytes(file, buffer, size);
        assert(readBytes == size);
        PHYSFS_close(file);

        vk::ShaderModuleCreateInfo smci;
        smci.codeSize = size;
        smci.pCode = (const uint32_t*)buffer;

        auto mod = dev.createShaderModule(smci);

        std::free(buffer);

        modules.insert({ id, mod });

        logMsg("loading shader %s from disk", AssetDB::idToPath(id).c_str());

        return mod;
    }

    void ShaderCache::clear() {
        for (auto& p : modules) {
            device.destroyShaderModule(p.second);
        }

        modules.clear();
    }
}
