#include <R2/BindlessTextureManager.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKTexture.hpp>
#include <Render/IRenderPipeline.hpp>
#include <Render/RenderInternal.hpp>
#include <Tracy.hpp>

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
    }

    VKRTTPass::~VKRTTPass()
    {
        renderer->bindlessTextureManager->FreeTextureHandle(finalTargetBindlessID);
        renderer->core->DestroyTexture(finalTarget);
        delete pipeline;
    }

    void VKRTTPass::setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
    {
        pipeline->setView(viewIndex, viewMatrix, projectionMatrix);
    }

    float* VKRTTPass::getHDRData()
    {
        return nullptr;
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