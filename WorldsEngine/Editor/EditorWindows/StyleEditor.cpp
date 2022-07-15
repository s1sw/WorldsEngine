#include "../../ImGui/imgui.h"
#include "EditorWindows.hpp"

namespace worlds
{
    void StyleEditor::draw(entt::registry &reg)
    {
        if (ImGui::Begin("Style Editor", &active))
        {
            ImGui::ShowStyleEditor();
        }
        ImGui::End();
    }
}
