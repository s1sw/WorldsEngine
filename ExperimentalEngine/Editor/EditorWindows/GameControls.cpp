#include "EditorWindows.hpp"
#include "imgui.h"
#include "IconsFontAwesome5.h"
#include "Console.hpp"

namespace worlds {
    void GameControls::draw(entt::registry& reg) {
        if (ImGui::Begin(ICON_FA_GAMEPAD u8" Game Controls", &active)) {
            if (ImGui::Button((const char*)(ICON_FA_PLAY_CIRCLE u8" Play"))) {
                g_console->executeCommandStr("play");
            }
        }
        ImGui::End();
    }
}