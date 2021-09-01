#include "Export.hpp"
#include "VR/IVRInterface.hpp"

using namespace worlds;
IVRInterface* csharpVrInterface;

extern "C" {
    EXPORT bool vr_enabled() {
        return csharpVrInterface != nullptr;
    }

    EXPORT void vr_getHeadTransform(float predictionTime, Transform* transform) {
        transform->fromMatrix(csharpVrInterface->getHeadTransform(predictionTime));
    }

    EXPORT void vr_getHandTransform(Hand hand, Transform* transform) {
        csharpVrInterface->getHandTransform(hand, *transform);
    }

    EXPORT InputActionHandle vr_getActionHandle(const char* actionPath) {
        return csharpVrInterface->getActionHandle(actionPath);
    }

    EXPORT bool vr_getActionHeld(InputActionHandle handle) {
        return csharpVrInterface->getActionHeld(handle);
    }

    EXPORT bool vr_getActionPressed(InputActionHandle handle) {
        return csharpVrInterface->getActionPressed(handle);
    }

    EXPORT bool vr_getActionReleased(InputActionHandle handle) {
        return csharpVrInterface->getActionReleased(handle);
    }

    EXPORT void vr_getActionVector2(InputActionHandle handle, glm::vec2* v2) {
        *v2 = csharpVrInterface->getActionV2(handle);
    }

    EXPORT void vr_triggerHaptics(InputActionHandle handle, float timeFromNow, float duration, float frequency, float amplitude) {
        csharpVrInterface->triggerHaptics(handle, timeFromNow, duration, frequency, amplitude);
    }
}
