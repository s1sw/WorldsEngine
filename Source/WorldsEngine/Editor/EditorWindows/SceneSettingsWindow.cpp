#include "../../Core/Engine.hpp"
#include "../GuiUtil.hpp"
#include "EditorWindows.hpp"

namespace worlds
{
    void SceneSettingsWindow::draw(entt::registry& reg)
    {
        if (ImGui::Begin("Scene Settings", &active))
        {
            auto& settings = reg.ctx<SceneSettings>();

            ImGui::Text("Current Skybox: %s", AssetDB::idToPath(settings.skybox).c_str());
            ImGui::SameLine();

            bool open = ImGui::Button("Change");
            selectAssetPopup("Change Skybox", settings.skybox, open);

            ImGui::DragFloat("Boost", &settings.skyboxBoost);
        }
        ImGui::End();
    }
}
