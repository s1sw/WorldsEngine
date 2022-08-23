#include "../../Core/Console.hpp"
#include "../../ImGui/imgui.h"
#include "../../Libs/IconsFontAwesome5.h"
#include "EditorWindows.hpp"

namespace worlds
{
    void GameControls::draw(entt::registry& reg)
    {
        if (ImGui::Begin(ICON_FA_GAMEPAD u8" Game Controls", &active))
        {

            switch (editor->getCurrentState())
            {
            case GameState::Editing:
                if (ImGui::Button(ICON_FA_PLAY_CIRCLE u8" Play"))
                {
                    g_console->executeCommandStr("play");
                }
                break;
            case GameState::Playing:
                if (ImGui::Button(ICON_FA_STOP_CIRCLE u8" Stop"))
                {
                    g_console->executeCommandStr("reloadAndEdit");
                }

                ImGui::SameLine();

                if (ImGui::Button(ICON_FA_PAUSE_CIRCLE u8" Pause"))
                {
                    g_console->executeCommandStr("pauseAndEdit");
                }
                break;
            case GameState::Paused:
                if (ImGui::Button(ICON_FA_STOP_CIRCLE u8" Stop"))
                {
                    g_console->executeCommandStr("reloadAndEdit");
                }

                ImGui::SameLine();

                if (ImGui::Button(ICON_FA_PAUSE_CIRCLE u8" Unpause"))
                {
                    g_console->executeCommandStr("unpause");
                }
                break;
            }
        }
        ImGui::End();
    }
}
