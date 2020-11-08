#include "EditorWindows.hpp"
#include "imgui.h"

namespace worlds {
    void StyleEditor::draw(entt::registry& reg) {
        if (ImGui::Begin("Style Editor", &active)) {
            ImGui::ShowStyleEditor();
        }
        ImGui::End();
    }
}