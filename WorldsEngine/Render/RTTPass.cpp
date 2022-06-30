#include <Render/RenderInternal.hpp>
#include <R2/VKTexture.hpp>
#include <R2/VKCore.hpp>
#include <R2/BindlessTextureManager.hpp>

using namespace R2::VK;

namespace worlds {
    VKRTTPass::VKRTTPass(VKRenderer* renderer, const RTTPassCreateInfo& ci)
        : renderer(renderer) {
        TextureCreateInfo tci = TextureCreateInfo::Texture2D(TextureFormat::R8G8B8A8_SRGB, ci.width, ci.height);
        tci.IsRenderTarget = true;
        sdrTarget = renderer->core->CreateTexture(tci);
        sdrTargetId = renderer->textureManager->AllocateTextureHandle(sdrTarget);

        width = ci.width;
        height = ci.height;
    }

    VKRTTPass::~VKRTTPass() {
        renderer->core->DestroyTexture(sdrTarget);
    }

    void VKRTTPass::drawNow(entt::registry& world) {
    }

    void VKRTTPass::requestPick(int x, int y) {
    }

    bool VKRTTPass::getPickResult(uint32_t* result) {
        return false;
    }

    float* VKRTTPass::getHDRData() {
        return nullptr;
    }

    void VKRTTPass::resize(int newWidth, int newHeight) {
    }

    void VKRTTPass::setResolutionScale(float newScale) {
    }

    ImTextureID VKRTTPass::getUITextureID() {
        return (ImTextureID)sdrTargetId;
    }
}