#include "EditorWindows.hpp"

namespace worlds {
    void AssetEditor::draw(entt::registry& reg) {
        if (ImGui::Begin("Asset Editor", &active)) {
            if (editor->currentSelectedAsset == INVALID_ASSET) {
                ImGui::Text("No asset currently selected.");
            } else {
            }
        }
        ImGui::End();
    }
}
