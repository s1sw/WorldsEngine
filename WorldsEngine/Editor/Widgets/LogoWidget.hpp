#pragma once
#include <ImGui/imgui.h>
#include <Core/IGameEventHandler.hpp>
#include <Render/Render.hpp>
#include <Core/AssetDB.hpp>

namespace worlds {
    class LogoWidget {
    public:
        LogoWidget(EngineInterfaces interfaces) {
            auto texMan = interfaces.renderer->getUITextureManager();
            background = texMan->loadOrGet(AssetDB::pathToId("UI/Editor/Images/worlds_no_logo.png"));
        }

        void draw() {
            glm::vec2 logoSize{494, 174};
            glm::vec2 screenCursorPos = (glm::vec2)ImGui::GetCursorScreenPos() + glm::vec2(ImGui::GetContentRegionAvailWidth() / 2.0f - logoSize.x / 2.0f, 0.0f);
            auto corner = screenCursorPos + logoSize;
            auto* drawList = ImGui::GetWindowDrawList();

            drawList->AddImage(background, screenCursorPos, corner);

            // draw orbit circles
            auto center = screenCursorPos + glm::vec2(174, 60);
            drawList->AddCircleFilled(center, 6.0f, ImColor(1.0f, 1.0f, 1.0f), 24);
            drawList->AddCircle(center, 20.0f, ImColor(1.0f, 1.0f, 1.0f), 32, 3.0f);
            drawList->AddCircle(center, 35.0f, ImColor(1.0f, 1.0f, 1.0f), 32, 3.0f);
            double currTime = ImGui::GetTime();
            currTime *= 0.25f;

            // draw planets
            auto innerPos = center + glm::vec2(glm::sin(currTime), glm::cos(currTime)) * 20.0f;
            drawList->AddCircleFilled(innerPos, 4.0f, ImColor(1.0f, 1.0f, 1.0f), 32);

            currTime *= 1.25f;

            auto outerPos = center + glm::vec2(glm::sin(currTime), glm::cos(currTime)) * 35.0f;
            drawList->AddCircleFilled(outerPos, 4.0f, ImColor(1.0f, 1.0f, 1.0f), 32);
            ImGui::Dummy(logoSize);
        }
    private:
        ImTextureID background;
    };
}