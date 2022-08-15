#include <R2/BindlessTextureManager.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKTexture.hpp>
#include <Render/IRenderPipeline.hpp>
#include <Render/RenderInternal.hpp>

using namespace R2::VK;

namespace worlds
{
    VKRTTPass::VKRTTPass(VKRenderer* renderer, const RTTPassCreateInfo& ci, IRenderPipeline* pipeline)
        : renderer(renderer), pipeline(pipeline)
    {
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
        renderer->core->DestroyTexture(finalTarget);
        delete pipeline;
    }

    void VKRTTPass::setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
    {
        pipeline->setView(viewIndex, viewMatrix, projectionMatrix);
    }

    void VKRTTPass::drawNow(entt::registry& world)
    {
    }

    void VKRTTPass::requestPick(int x, int y)
    {
    }

    bool VKRTTPass::getPickResult(uint32_t* result)
    {
        return false;
    }

    float* VKRTTPass::getHDRData()
    {
        return nullptr;
    }

    void VKRTTPass::resize(int newWidth, int newHeight)
    {
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

    void VKRTTPass::setResolutionScale(float newScale)
    {
    }

    ImTextureID VKRTTPass::getUITextureID()
    {
        return (ImTextureID)(uint64_t)finalTargetBindlessID;
    }

    const RTTPassCreateInfo& VKRTTPass::getSettings()
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