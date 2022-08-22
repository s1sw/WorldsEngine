#include "EditorWindows.hpp"
#include <Render/Render.hpp>
#include <Libs/IconsFontAwesome5.h>

namespace worlds
{
    RTTPass* gameRTTPass = nullptr;
    uint32_t currentWidth;
    uint32_t currentHeight;
    bool setIgnoreImGui = false;
    
    void GameView::draw(entt::registry& reg)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(256.0f, 256.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::Begin(ICON_FA_GAMEPAD u8" Game View"))
        {
            if (ImGui::IsWindowHovered())
            {
                interfaces.inputManager->setImGuiIgnore(true);
                setIgnoreImGui = true;
            }

            ImVec2 contentRegion = ImGui::GetContentRegionAvail();

            if (gameRTTPass == nullptr)
            {
                currentWidth = contentRegion.x;
                currentHeight = contentRegion.y;

                RTTPassSettings gameViewPassCI
                {
                    .cam = interfaces.mainCamera,
                    .width = currentWidth,
                    .height = currentHeight,
                    .useForPicking = false,
                    .enableShadows = true,
                    .msaaLevel = 4
                };

                gameRTTPass = interfaces.renderer->createRTTPass(gameViewPassCI);
            }

            gameRTTPass->active = true;

            if ((contentRegion.x != currentWidth || contentRegion.y != currentHeight) && contentRegion.x > 256 &&
                contentRegion.y > 256)
            {
                currentWidth = contentRegion.x;
                currentHeight = contentRegion.y;
                gameRTTPass->resize(currentWidth, currentHeight);
            }

            ImGui::Image(gameRTTPass->getUITextureID(), ImVec2(currentWidth, currentHeight));
        }
        else
        {
            gameRTTPass->active = false;
            if (setIgnoreImGui)
            {
                interfaces.inputManager->setImGuiIgnore(false);
                setIgnoreImGui = false;
            }
        }
        ImGui::End();
    }
}