#include <R2/BindlessTextureManager.hpp>
#include <R2/VK.hpp>
#include <R2/VKSyncPrims.hpp>
#include <Render/IRenderPipeline.hpp>
#include <Render/RenderInternal.hpp>
#include <Tracy.hpp>
#include <stdexcept>

using namespace R2::VK;

namespace worlds
{
    VKRTTPass::VKRTTPass(VKRenderer* renderer, const RTTPassSettings& ci, IRenderPipeline* pipeline)
        : renderer(renderer), pipeline(pipeline)
    {
        ZoneScoped;
        TextureCreateInfo tci = TextureCreateInfo::Texture2D(TextureFormat::R8G8B8A8_SRGB, ci.width, ci.height);

        if (ci.numViews > 1)
        {
            tci.Dimension = TextureDimension::Array2D;
            tci.Layers = ci.numViews;
        }

        tci.IsRenderTarget = true;
        finalTarget = renderer->core->CreateTexture(tci);
        finalTargetBindlessID = renderer->bindlessTextureManager->AllocateTextureHandle(finalTarget);

        settings = ci;
        width = settings.width;
        height = settings.height;
        cam = settings.cam;

        if (settings.enableHDRCapture)
        {
            TextureCreateInfo tci = TextureCreateInfo::Texture2D(TextureFormat::R32G32B32A32_SFLOAT, width, height);
            tci.CanUseAsStorage = false;

            requestedHdrResult = renderer->getCore()->CreateTexture(tci);
            BufferCreateInfo bci { BufferUsage::Storage, sizeof(float) * 4 * width * height, true };
            hdrDataBuffer = renderer->getCore()->CreateBuffer(bci);
            captureEvent = new Event(renderer->getCore());
            captureEvent->Reset();
        }
    }

    VKRTTPass::~VKRTTPass()
    {
        renderer->bindlessTextureManager->FreeTextureHandle(finalTargetBindlessID);
        renderer->core->DestroyTexture(finalTarget);

        if (settings.enableHDRCapture)
            renderer->core->DestroyTexture(requestedHdrResult);
        delete pipeline;
    }

    void VKRTTPass::setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
    {
        pipeline->setView(viewIndex, viewMatrix, projectionMatrix);
    }

    void VKRTTPass::downloadHDROutput(R2::VK::CommandBuffer& cb)
    {
        Texture* hdrTex = pipeline->getHDRTexture();

        TextureBlit blitInfo{};
        blitInfo.Source.LayerCount = 1;
        blitInfo.Destination.LayerCount = 1;
        blitInfo.SourceOffsets[1] = blitInfo.DestinationOffsets[1] = BlitOffset{ (int)width, (int)height, 1 };

        cb.TextureBlit(hdrTex, requestedHdrResult, blitInfo);
        cb.TextureCopyToBuffer(requestedHdrResult, hdrDataBuffer);
        cb.SetEvent(captureEvent);

        hdrDataReady = true;
        hdrDataRequested = false;
    }

    void VKRTTPass::requestHDRData()
    {
        if (settings.msaaLevel != 1) throw std::runtime_error("Can't download HDR data from an MSAA RTTPass");
        hdrDataRequested = true;
    }

    bool VKRTTPass::getHDRData(float*& out)
    {
        if (!hdrDataReady || !captureEvent->IsSet())
            return false;

        captureEvent->Reset();
        
        hdrDataReady = false;
        float* data = (float*)malloc(sizeof(float) * 4 * width * height);

        float* src = (float*)hdrDataBuffer->Map();
        memcpy(data, src, sizeof(float) * 4 * width * height);
        hdrDataBuffer->Unmap();

        out = data;

        return true;
    }

    void VKRTTPass::resize(int newWidth, int newHeight)
    {
        ZoneScoped;
        TextureCreateInfo tci = TextureCreateInfo::Texture2D(TextureFormat::R8G8B8A8_SRGB, newWidth, newHeight);
        tci.IsRenderTarget = true;
        tci.Layers = settings.numViews;
        renderer->core->DestroyTexture(finalTarget);

        finalTarget = renderer->core->CreateTexture(tci);
        renderer->bindlessTextureManager->SetTextureAt(finalTargetBindlessID, finalTarget);

        settings.width = newWidth;
        settings.height = newHeight;
        width = settings.width;
        height = settings.height;

        pipeline->onResize(newWidth, newHeight);
    }

    ImTextureID VKRTTPass::getUITextureID()
    {
        return (ImTextureID)(uint64_t)finalTargetBindlessID;
    }

    const RTTPassSettings& VKRTTPass::getSettings()
    {
        return settings;
    }

    R2::VK::Texture* VKRTTPass::getFinalTarget()
    {
        return finalTarget;
    }

    Camera* VKRTTPass::getCamera()
    {
        return cam;
    }
}