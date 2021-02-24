#include "../../Core/Engine.hpp"
#include "EditorWindows.hpp"
#include "../../ImGui/imgui.h"
#include "../../Libs/IconsFontAwesome5.h"
#include "../../Libs/IconsFontaudio.h"
#include "Util/EnumUtil.hpp"

namespace worlds {
    void BakingWindow::draw(entt::registry& reg) {
        if (ImGui::Begin(ICON_FA_COOKIE u8" Baking")) {
            if (ImGui::CollapsingHeader(ICON_FAD_SPEAKER u8" Audio")) {
                uint32_t staticAudioGeomCount = 0;

                reg.view<WorldObject>().each([&](auto, WorldObject& wo) {
                    if (enumHasFlag(wo.staticFlags, StaticFlags::Audio))
                        staticAudioGeomCount++;
                });

                if (staticAudioGeomCount > 0) {
                    ImGui::Text("%u static geometry objects", staticAudioGeomCount);
                } else {
                    ImGui::TextColored(ImColor(1.0f, 0.0f, 0.0f), "There aren't any objects marked as audio static in the scene.");
                }
            }
        }
        ImGui::End();
    }
}
