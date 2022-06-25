#include <Render/RenderInternal.hpp>

namespace worlds {
    VKRTTPass::VKRTTPass(VKRenderer* renderer)
        : renderer(renderer) {
    }

    VKRTTPass::~VKRTTPass() {
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
}