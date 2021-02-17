#include "EditorWindows.hpp"
#include "../../ImGui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../../ImGui/imgui_internal.h"

namespace worlds {
    ImTextureID bgId = nullptr;
    void AboutWindow::setActive(bool active) {
        this->active = active;
        timeAtOpen = ImGui::GetTime();
        bgId = editor->texManager()->loadOrGet(g_assetDB.addOrGetExisting("UI/Images/worlds_no_logo.png"));
    }

    void AboutWindow::draw(entt::registry&) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        if (ImGui::Begin("About", &active)) {
            auto cursorPos = ImGui::GetCursorPos() + ImGui::GetWindowPos();
            auto corner = cursorPos + ImVec2(494, 174);
            auto* drawList = ImGui::GetWindowDrawList();

            drawList->AddImage(bgId, cursorPos, corner);
            
            // draw orbit circles
            auto center = cursorPos + ImVec2(174, 60);
            drawList->AddCircleFilled(center, 6.0f, ImColor(1.0f, 1.0f, 1.0f), 24);
            drawList->AddCircle(center, 20.0f, ImColor(1.0f, 1.0f, 1.0f), 32, 3.0f);
            drawList->AddCircle(center, 35.0f, ImColor(1.0f, 1.0f, 1.0f), 32, 3.0f);
            double currTime = ImGui::GetTime();
            currTime *= 0.25f;

            // draw planets
            auto innerPos = center + ImVec2(glm::sin(currTime), glm::cos(currTime)) * 20.0f;
            drawList->AddCircleFilled(innerPos, 4.0f, ImColor(1.0f, 1.0f, 1.0f), 32);

            currTime *= 1.25f;

            auto outerPos = center + ImVec2(glm::sin(currTime), glm::cos(currTime)) * 35.0f;
            drawList->AddCircleFilled(outerPos, 4.0f, ImColor(1.0f, 1.0f, 1.0f), 32);
        }
        ImGui::End(); 
        ImGui::PopStyleColor();
    }
}
