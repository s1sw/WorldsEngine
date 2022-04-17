#include "IntegratedMenubar.hpp"
#include <Core/Window.hpp>
#include <Core/ConVar.hpp>
#include <Core/Engine.hpp>
#include <ImGui/imgui.h>

namespace worlds {
    static ConVar integratedMenuBar{ "ed_integratedMenuBar", "0" };
    IntegratedMenubar::IntegratedMenubar(EngineInterfaces interfaces) : interfaces(interfaces) {}

    void IntegratedMenubar::draw() {
        const char* windowTitle = interfaces.engine->getMainWindow().getTitle();
        glm::vec2 textSize = ImGui::CalcTextSize(windowTitle);
        glm::vec2 menuBarCenter = ImGui::GetWindowSize();
        menuBarCenter *= 0.5f;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (integratedMenuBar.getInt()) {
            drawList->AddText(ImVec2(menuBarCenter.x - (textSize.x * 0.5f), ImGui::GetWindowHeight() * 0.15f), ImColor(255, 255, 255), windowTitle);

            SDL_SetWindowBordered(interfaces.engine->getMainWindow().getWrappedHandle(), SDL_FALSE);
            float barWidth = ImGui::GetWindowWidth();
            float barHeight = ImGui::GetWindowHeight();
            const float crossSize = 6.0f;
            glm::vec2 crossCenter(ImGui::GetWindowWidth() - 17.0f - crossSize, menuBarCenter.y);
            crossCenter -= glm::vec2(0.5f, 0.5f);
            auto crossColor = ImColor(255, 255, 255);

            ImVec2 mousePos = ImGui::GetMousePos();

            if (mousePos.x > barWidth - 45.0f && mousePos.y < barHeight) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    interfaces.engine->quit();
                }
                drawList->AddRectFilled(ImVec2(barWidth - 45.0f, 0.0f), ImVec2(barWidth, barHeight), ImColor(255, 0, 0, 255));
            }

            drawList->AddLine(crossCenter + glm::vec2(+crossSize, +crossSize), crossCenter + glm::vec2(-crossSize, -crossSize), crossColor, 1.0f);
            drawList->AddLine(crossCenter + glm::vec2(+crossSize, -crossSize), crossCenter + glm::vec2(-crossSize, +crossSize), crossColor, 1.0f);

            Window& window = interfaces.engine->getMainWindow();
            glm::vec2 maximiseCenter(barWidth - 45.0f - 22.0f, menuBarCenter.y);
            if (mousePos.x > barWidth - 90.0f && mousePos.x < barWidth - 45.0f && mousePos.y < barHeight) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (window.isMaximised())
                        window.restore();
                    else
                        window.maximise();
                }
                drawList->AddRectFilled(ImVec2(barWidth - 45.0f - 45.0f, 0.0f), ImVec2(barWidth - 45.0f, barHeight), ImColor(255, 255, 255, 50));
            }

            if (!window.isMaximised()) {
                drawList->AddRect(maximiseCenter - glm::vec2(crossSize, crossSize), maximiseCenter + glm::vec2(crossSize, crossSize), ImColor(255, 255, 255));
            } else {
                drawList->AddRect(maximiseCenter - glm::vec2(crossSize - 3, crossSize), maximiseCenter + glm::vec2(crossSize, crossSize - 3), ImColor(255, 255, 255));
                drawList->AddRectFilled(maximiseCenter - glm::vec2(crossSize, crossSize - 3), maximiseCenter + glm::vec2(crossSize - 3, crossSize), ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                drawList->AddRect(maximiseCenter - glm::vec2(crossSize, crossSize - 3), maximiseCenter + glm::vec2(crossSize - 3, crossSize), ImColor(255, 255, 255));
            }

            glm::vec2 minimiseCenter(barWidth - 90.0f - 22.0f, menuBarCenter.y);
            if (mousePos.x > barWidth - 135.0f && mousePos.x < barWidth - 90.0f && mousePos.y < barHeight) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {

                    if (window.isMinimised())
                        window.restore();
                    else
                        window.minimise();
                }
                drawList->AddRectFilled(ImVec2(barWidth - 135.0f, 0.0f), ImVec2(barWidth - 90.0f, barHeight), ImColor(255, 255, 255, 50));
            }
            drawList->AddRectFilled(minimiseCenter - glm::vec2(5, 0), minimiseCenter + glm::vec2(5, 1), ImColor(255, 255, 255));
        } else {
            SDL_SetWindowBordered(interfaces.engine->getMainWindow().getWrappedHandle(), SDL_TRUE);
        }
    }
}