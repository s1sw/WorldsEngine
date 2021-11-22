#include "ShaderCache.hpp"
#include "Core/Fatal.hpp"
#include <Core/AssetDB.hpp>
#include <string>

namespace worlds {
    std::unordered_map<AssetID, VkShaderModule> ShaderCache::modules;
    VkDevice ShaderCache::device;
    VkShaderModule ShaderCache::getModule(VkDevice& dev, AssetID id) {
        auto it = modules.find(id);

        if (it != modules.end())
            return it->second;

        PHYSFS_File* file = worlds::AssetDB::openAssetFileRead(id);

        if (!file) {
            std::string msg = "Failed to open shader file" + AssetDB::idToPath(id);
            fatalErr(msg.c_str());
        }

        size_t size = PHYSFS_fileLength(file);
        void* buffer = std::malloc(size);

        size_t readBytes = PHYSFS_readBytes(file, buffer, size);
        if (readBytes != size) {
            fatalErr("Failed to read whole shader file");
        }
        PHYSFS_close(file);

        VkShaderModuleCreateInfo smci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        smci.codeSize = size;
        smci.pCode = (const uint32_t*)buffer;

        VkShaderModule mod;
        VKCHECK(vkCreateShaderModule(dev, &smci, nullptr, &mod));

        std::string path = AssetDB::idToPath(id);

        vku::setObjectName(device, (uint64_t)mod, VK_OBJECT_TYPE_SHADER_MODULE, path.c_str());

        std::free(buffer);

        modules.insert({ id, mod });

        logVrb(WELogCategoryRender, "loading shader %s from disk", AssetDB::idToPath(id).c_str());

        return mod;
    }

    void ShaderCache::clear() {
        for (auto& p : modules) {
            vkDestroyShaderModule(device, p.second, nullptr);
        }

        modules.clear();
    }
}
