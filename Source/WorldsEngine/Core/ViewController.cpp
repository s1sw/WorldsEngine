#include "ViewController.hpp"
#include <Core/Engine.hpp>
#include <Core/Window.hpp>
#include <Render/Render.hpp>
#include <ImGui/imgui.h>
#include <VR/OpenXRInterface.hpp>

namespace worlds
{
    ViewController::ViewController(const EngineInterfaces& interfaces, MainViewSettings settings)
        : interfaces(interfaces)
        , xrEnabled(settings.xrEnabled)
        , boundWindow(settings.boundWindow)
    {
        RTTPassSettings passSettings
        {
            .cam = interfaces.mainCamera,
            .enableShadows = true,
            .msaaLevel = 4,
            .numViews = settings.xrEnabled ? 2 : 1,
            .outputToXR = settings.xrEnabled,
            .setViewsFromXR = settings.xrEnabled
        };
        getViewResolution(&passSettings.width, &passSettings.height);

        rttPass = interfaces.renderer->createRTTPass(passSettings);
        rttPass->active = true;
    }

    ViewController::~ViewController()
    {
        interfaces.renderer->destroyRTTPass(rttPass);
    }

    void ViewController::draw()
    {
        resizeIfNecessary();

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        ImTextureID id = rttPass->getUITextureID();
        drawList->AddImage(id, ImVec2(0, 0), ImVec2(rttPass->width, rttPass->height));
    }

    void ViewController::getViewResolution(uint32_t* width, uint32_t* height)
    {
        if (!xrEnabled)
        {
            int iWidth, iHeight;
            boundWindow->getSize(&iWidth, &iHeight);
            *width = iWidth;
            *height = iHeight;
        }
        else
        {
            interfaces.vrInterface->getRenderResolution(width, height);
        }
    }

    void ViewController::resizeIfNecessary()
    {
        uint32_t w, h;
        getViewResolution(&w, &h);

        if (w != rttPass->width || h != rttPass->height)
        {
            rttPass->resize(w, h);
        }
    }
}