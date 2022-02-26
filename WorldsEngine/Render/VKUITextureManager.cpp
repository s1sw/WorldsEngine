#include "RenderInternal.hpp"
#include "Loaders/TextureLoader.hpp"
#include "vku/vku.hpp"
#include "Util/VKImGUIUtil.hpp"

namespace worlds {
    VKUITextureManager::VKUITextureManager(VKRenderer* renderer, const VulkanHandles& handles)
        : handles{ handles }
        , renderer{ renderer } {
    }

    VKUITextureManager::~VKUITextureManager() {
        for (auto& p : texInfo) {
            VKImGUIUtil::destroyDescriptorSet(p.second->ds, &handles);
            delete p.second;
        }

        texInfo.clear();
    }

    void VKUITextureManager::unload(AssetID id) {
        auto it = texInfo.find(id);

        if (it == texInfo.end()) return;

        VKImGUIUtil::destroyDescriptorSet(it->second->ds, &handles);
        texInfo.erase(id);
    }

    ImTextureID VKUITextureManager::loadOrGet(AssetID id) {
        auto it = texInfo.find(id);

        UITexInfo* ti;
        if (it == texInfo.end()) {
            ti = load(id);
            texInfo.insert({ id, ti });
        } else {
            ti = it->second;
        }

        return (ImTextureID)ti->ds;
    }

    VKUITextureManager::UITexInfo* VKUITextureManager::load(AssetID id) {
        auto tData = loadTexData(id);

        if (tData.data == nullptr)
            logErr("Failed to load UI image %s", AssetDB::idToPath(id).c_str());

        std::lock_guard<std::mutex> lg{ *renderer->apiMutex };
        vku::TextureImage2D t2d = uploadTextureVk(handles, tData);

        auto texInfo = new UITexInfo;
        texInfo->image = std::move(t2d);
        texInfo->ds = VKImGUIUtil::createDescriptorSetFor(texInfo->image, &handles);

        return texInfo;
    }
}
