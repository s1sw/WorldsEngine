#include "EditorWindows.hpp"
#include "../../ImGui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../../ImGui/imgui_internal.h"

namespace worlds {
    ImTextureID bgId = nullptr;
    void AboutWindow::setActive(bool active) {
        this->active = active;
        bgId = editor->texManager()->loadOrGet(g_assetDB.addOrGetExisting("UI/Images/worlds_no_logo.png"));
    }

    void AboutWindow::draw(entt::registry&) {
        //ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        if (ImGui::Begin("About", &active)) {
            auto screenCursorPos = ImGui::GetCursorPos() + ImGui::GetWindowPos() - ImVec2(0.0f, ImGui::GetScrollY());
            auto corner = screenCursorPos + ImVec2(494, 174);
            auto* drawList = ImGui::GetWindowDrawList();

            drawList->AddImage(bgId, screenCursorPos, corner);
            
            // draw orbit circles
            auto center = screenCursorPos + ImVec2(174, 60);
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

            ImGui::SetCursorPos(ImGui::GetCursorStartPos() + ImVec2(0, 174 + 5));
            ImGui::Text("Programmed by Someone Somewhere :)");
            ImGui::Text("Open source libraries:");
            ImGui::Text(" - EnTT");
            ImGui::Text(" - PhysX");
            ImGui::Text(" - Dear ImGUI");
            ImGui::Text(" - stb_image");
            ImGui::Text(" - stb_vorbis");
            ImGui::Text(" - sajson");
            ImGui::Text(" - crunch");
            ImGui::Text(" - VulkanMemoryAllocator");
            ImGui::Text(" - moodycamel readerwriterqueue");
            ImGui::Text(" - tinyobjloader");
            ImGui::Text(" - MikkTSpace");
            ImGui::Text(" - SDL2");
            ImGui::Text(" - Wren");
            ImGui::Text(" - ENet");
            ImGui::Text(" - PhysFS");
            ImGui::Text(" - Tracy Profiler");
            ImGui::Text(" - Vookoo (sorta, it's been rewritten by now :P)");

            auto cursorX = ImGui::GetCursorStartPos().x + 375;

            ImGui::SetCursorPos(ImGui::GetCursorStartPos() + ImVec2(375, 174 + 5 + ImGui::GetTextLineHeightWithSpacing()));
            ImGui::Text("Thanks to:");
            ImGui::SetCursorPosX(cursorX);
            ImGui::Text(" - The VR Physics Developers Discord server");
            ImGui::SetCursorPosX(cursorX);
            ImGui::Text(" - Maranara for testing");
        }
        ImGui::End(); 
        //ImGui::PopStyleColor();
    }
}
