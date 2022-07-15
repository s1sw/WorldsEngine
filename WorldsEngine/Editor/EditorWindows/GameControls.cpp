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
            if (ImGui::Button((const char*)(ICON_FA_PLAY_CIRCLE u8" Play")))
            {
                g_console->executeCommandStr("play");
            }
        }
        ImGui::End();
    }
}
