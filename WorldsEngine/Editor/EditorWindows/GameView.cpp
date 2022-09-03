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
        bool isVR = interfaces.vrInterface != nullptr;
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
                    .msaaLevel = 4,
                    .numViews = isVR ? 2 : 1,
                    .outputToXR = isVR
                };

                gameRTTPass = interfaces.renderer->createRTTPass(gameViewPassCI);
            }

            gameRTTPass->active = true;

            if ((contentRegion.x != currentWidth || contentRegion.y != currentHeight) && 
                contentRegion.x > 256 && contentRegion.y > 256)
            {
                currentWidth = contentRegion.x;
                currentHeight = contentRegion.y;
                if (!isVR)
                {
                    gameRTTPass->resize(currentWidth, currentHeight);
                }
            }

            if (isVR)
            {
                uint32_t vrWidth, vrHeight;
                interfaces.vrInterface->getRenderResolution(&vrWidth, &vrHeight);

                if (gameRTTPass->width != vrWidth || gameRTTPass->height != vrHeight)
                {
                    gameRTTPass->resize((int)vrWidth, (int)vrHeight);
                }
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

        if (isVR)
        {
            if (editor->isPlaying() && interfaces.renderer->getVsync())
            {
                interfaces.renderer->setVsync(false);
            }

            if (editor->isPlaying())
                interfaces.vrInterface->waitGetPoses();

            float predictAmount = interfaces.vrInterface->getPredictAmount();
            glm::mat4 ht = interfaces.vrInterface->getHeadTransform(predictAmount);
            interfaces.renderer->setVRUsedPose(ht);

            gameRTTPass->setView(
                0, glm::inverse(ht * interfaces.vrInterface->getEyeViewMatrix(Eye::LeftEye)),
                interfaces.vrInterface->getEyeProjectionMatrix(Eye::LeftEye, interfaces.mainCamera->near));

            gameRTTPass->setView(
                1, glm::inverse(ht * interfaces.vrInterface->getEyeViewMatrix(Eye::RightEye)),
                interfaces.vrInterface->getEyeProjectionMatrix(Eye::RightEye, interfaces.mainCamera->near));
        }
        ImGui::End();
    }
}