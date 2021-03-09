#pragma once
#include <string>
#include "../ImGui/imgui.h"
#include <functional>
#include "../ImGui/imgui_stdlib.h"

namespace worlds {
    typedef uint32_t AssetID;

    // Open with ImGui::OpenPopup(title)
    void saveFileModal(const char* title, std::function<void(const char*)> saveCallback);
    void openFileModal(const char* title, std::function<void(const char*)> openCallback, const char* fileExtension = nullptr, const char* startingDir = nullptr);
    void openFileModal(const char* title, std::function<void(const char*)> openCallback, const char** fileExtensions = nullptr, int fileExtensionCount = 0, const char* startingDir = nullptr);
    bool selectAssetPopup(const char* title, AssetID& id, bool open);
    void tooltipHover(const char* desc);
}
