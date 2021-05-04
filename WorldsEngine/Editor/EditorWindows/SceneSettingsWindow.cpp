#include "../../Core/Engine.hpp"
#include "EditorWindows.hpp"
#include "../GuiUtil.hpp"

namespace worlds {
    void SceneSettingsWindow::draw(entt::registry& reg) {
        if (ImGui::Begin("Scene Settings", &active)) {
            auto& settings = reg.ctx<SceneSettings>();

            ImGui::Text("Current Skybox: %s", g_assetDB.getAssetPath(settings.skybox).c_str());
            ImGui::SameLine();

            bool open = ImGui::Button("Change");
            selectAssetPopup("Change Skybox", settings.skybox, open);
        }
        ImGui::End();
    }
}
