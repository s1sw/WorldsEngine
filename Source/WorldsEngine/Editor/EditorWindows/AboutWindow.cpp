#include "EditorWindows.hpp"
#include "ImGui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "Audio/Audio.hpp"
#include "ImGui/imgui_internal.h"
#include "Render/Render.hpp"
#include <BuildInfo.hpp>

namespace worlds
{
    ImTextureID bgId = nullptr;
    ImTextureID someoneId = nullptr;

    void AboutWindow::setActive(bool active)
    {
        this->active = active;
        IUITextureManager* texMan = interfaces.renderer->getUITextureManager();
        bgId = texMan->loadOrGet(AssetDB::pathToId("UI/Editor/Images/worlds_no_logo.png"));
        someoneId = texMan->loadOrGet(AssetDB::pathToId("UI/Editor/Images/someone_avatar.png"));
    }

    void AboutWindow::draw(entt::registry&)
    {
        // ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        if (ImGui::Begin("About", &active))
        {
            ImVec2 logoSize{494, 174};
            auto screenCursorPos =
                ImGui::GetCursorScreenPos() + ImVec2(ImGui::GetWindowWidth() / 2.0f - logoSize.x / 2.0f, 0.0f);
            auto corner = screenCursorPos + logoSize;
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
            ImGui::Text("Worlds Engine v%s", WORLDS_VERSION);
            ImGui::Image(someoneId, ImVec2(32.0f, 32.0f));
            ImGui::SameLine();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
            ImGui::Text("Programmed by Someone Somewhere :)");
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6.0f);
            ImGui::Text("Open source libraries:");
            ImGui::Text(" - EnTT");
            ImGui::Text(" - PhysX");
            ImGui::Text(" - Dear ImGUI");
            ImGui::Text(" - stb_image");
            ImGui::Text(" - JSON for Modern C++");
            ImGui::Text(" - crunch");
            ImGui::Text(" - VulkanMemoryAllocator");
            ImGui::Text(" - moodycamel readerwriterqueue");
            ImGui::Text(" - tinyobjloader");
            ImGui::Text(" - MikkTSpace");
            ImGui::Text(" - SDL2");
            ImGui::Text(" - PhysFS");
            ImGui::Text(" - Tracy Profiler");

            auto cursorX = ImGui::GetCursorStartPos().x + 375;

            ImGui::SetCursorPos(ImGui::GetCursorStartPos() +
                                ImVec2(375, 174 + 5 + ImGui::GetTextLineHeightWithSpacing()));
            ImGui::Text("Thanks to:");

            {
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - PixHammer");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - Maranara");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - Tabloid");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - The Graphics Programming Discord server");
            }

            ImGui::SetCursorPosX(cursorX);
        }
        ImGui::End();
        // ImGui::PopStyleColor();
    }
}
