#pragma once
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_stdlib.h"
#include <entt/entity/lw_fwd.hpp>
#include <functional>
#include <string>

namespace worlds
{
    typedef uint32_t AssetID;
    const char *getIcon(const std::string &ext);

    // Open with ImGui::OpenPopup(title)
    void saveFileModal(const char *title, std::function<void(const char *)> saveCallback);
    void openFileModal(const char *title, std::function<void(const char *)> openCallback,
                       const char *fileExtension = nullptr, const char *startingDir = nullptr);
    void openFileModal(const char *title, std::function<void(const char *)> openCallback,
                       const char **fileExtensions = nullptr, int fileExtensionCount = 0,
                       const char *startingDir = nullptr);
    void openFileModalOffset(const char *title, std::function<void(const char *)> openCallback, const char *rootOffset,
                             const char **fileExtensions = nullptr, int fileExtensionCount = 0,
                             const char *startingDir = nullptr);

    void openFileFullFSModal(const char *title, std::function<void(const char *)> openCallback);

    enum class MessageBoxType
    {
        YesNo,
        Ok
    };

    void messageBoxModal(const char *title, const char *desc, std::function<void(bool)> callback,
                         MessageBoxType type = MessageBoxType::YesNo);

    void drawModals();

    enum class NotificationType
    {
        Info,
        Warning,
        Error
    };

    void addNotification(std::string text, NotificationType type = NotificationType::Info);
    void drawPopupNotifications();
    bool selectAssetPopup(const char *title, AssetID &id, bool open, bool source = false);
    bool selectRawAssetPopup(const char *title, AssetID &id, bool open);
    void tooltipHover(const char *desc);
    void pushBoldFont();
    void selectSceneEntity(const char *title, entt::registry &reg, std::function<void(entt::entity)> callback);

    namespace EditorUI
    {
        void centeredText(const char *text);
    }
}
