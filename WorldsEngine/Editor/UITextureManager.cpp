#include "UITextureManager.hpp"
#include "../Render/Loaders/TextureLoader.hpp"
#include "Render/vku/vku.hpp"
#include "../Util/VKImGUIUtil.hpp"

namespace worlds {
    struct UITextureManager::UITexInfo {
        vk::DescriptorSet ds;
        vku::TextureImage2D image;
    };

    UITextureManager::UITextureManager(const VulkanHandles& handles)
        : handles { handles } {
    }

    void UITextureManager::unload(AssetID id) {
        auto it = texInfo.find(id);

        if (it == texInfo.end()) return;

        VKImGUIUtil::destroyDescriptorSet(it->second->ds, &handles);
        texInfo.erase(id);
    }

    ImTextureID UITextureManager::loadOrGet(AssetID id) {
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

    UITextureManager::UITexInfo* UITextureManager::load(AssetID id) {
        auto tData = loadTexData(id);
        vku::TextureImage2D t2d = uploadTextureVk(handles, tData);

        auto texInfo = new UITexInfo;
        texInfo->image = std::move(t2d);
        texInfo->ds = VKImGUIUtil::createDescriptorSetFor(texInfo->image, &handles);

        return texInfo;
    }
}
