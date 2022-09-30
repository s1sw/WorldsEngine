#include <Render/RenderInternal.hpp>
#include <Core/Engine.hpp>
#include <R2/VK.hpp>

using namespace R2;

namespace worlds
{
    void XRPresentManager::createTextures()
    {
        VK::TextureCreateInfo tci =
            VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::R8G8B8A8_SRGB, width, height);
        // When CanUseAsStorage is true, the texture created is actually in UNORM format, but with an SRGB view.
        // This causes the blit below to incorrectly apply a linear->SRGB corretion.
        tci.CanUseAsStorage = false;
        leftEye = core->CreateTexture(tci);
        leftEye->SetDebugName("Left Eye Submit Texture");
        rightEye = core->CreateTexture(tci);
        rightEye->SetDebugName("Left Eye Submit Texture");
    }

    XRPresentManager::XRPresentManager(const EngineInterfaces& interfaces, int width, int height)
        : width(width)
        , height(height)
        , interfaces(interfaces)
        , core(((VKRenderer*)interfaces.renderer)->getCore())
    {
        if (width == 0 || height == 0)
        {
            uint32_t uWidth, uHeight;
            interfaces.vrInterface->getRenderResolution(&uWidth, &uHeight);
            this->width = (int)uWidth;
            this->height = (int)uHeight;
        }

        createTextures();
    }

    void XRPresentManager::resize(int width, int height)
    {
        this->width = width;
        this->height = height;
        createTextures();
    }

    void XRPresentManager::copyFromLayered(VK::CommandBuffer cb, VK::Texture* layeredTexture)
    {
        VK::TextureCopy leftCopy{};
        leftCopy.Source.LayerCount = 1;
        leftCopy.Destination.LayerCount = 1;
        leftCopy.Extent =
            VK::BlitExtent{(uint32_t)layeredTexture->GetWidth(), (uint32_t)layeredTexture->GetHeight(), 1};

        cb.TextureCopy(layeredTexture, leftEye.Get(), leftCopy);

        VK::TextureCopy rightCopy{};
        rightCopy.Source.LayerCount = 1;
        rightCopy.Source.LayerStart = 1;
        rightCopy.Destination.LayerCount = 1;
        rightCopy.Extent = leftCopy.Extent;

        cb.TextureCopy(layeredTexture, rightEye.Get(), rightCopy);
    }

    void XRPresentManager::preSubmit()
    {
        uint32_t uWidth, uHeight;
        interfaces.vrInterface->getRenderResolution(&uWidth, &uHeight);
        if ((int)uWidth != width || (int)uHeight != height)
        {
            resize((int)uWidth, (int)uHeight);
        }

        interfaces.vrInterface->submitExplicitTimingData();
    }

    void XRPresentManager::submit(glm::mat4 usedPose)
    {
        VRSubmitInfo submitInfo{};
        submitInfo.renderPose = usedPose;
        submitInfo.leftEye = leftEye.Get();
        submitInfo.rightEye = rightEye.Get();
        submitInfo.vkHandles = core->GetHandles();
        interfaces.vrInterface->submit(submitInfo);
    }
}