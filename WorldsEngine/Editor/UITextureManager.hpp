#pragma once
#include "../Core/AssetDB.hpp"
#include "../ImGui/imgui.h"
#include <robin_hood.h>

// UI texture manager. Only supports stb_image.
namespace worlds {
    struct VulkanHandles;
    class UITextureManager {
    public:
        UITextureManager(const VulkanHandles& handles);
        ImTextureID loadOrGet(AssetID id);
        void unload(AssetID id);
        ~UITextureManager();
    private:
        struct UITexInfo;
        UITexInfo* load(AssetID id);
        const VulkanHandles& handles;
        robin_hood::unordered_map<AssetID, UITexInfo*> texInfo;
    };
}
