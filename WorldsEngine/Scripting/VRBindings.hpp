#include "Export.hpp"
#include "VR/OpenXRInterface.hpp"

using namespace worlds;
OpenXRInterface* csharpVrInterface;

extern "C"
{
    EXPORT bool vr_enabled()
    {
        return csharpVrInterface != nullptr;
    }

    EXPORT void vr_getHeadTransform(float predictionTime, Transform* transform)
    {
        const UnscaledTransform& ut = csharpVrInterface->getHmdTransform();
        transform->position = ut.position;
        transform->rotation = ut.rotation;
        transform->scale = glm::vec3{ 1.0f };
    }

    EXPORT void vr_getHandTransform(Hand hand, Transform* transform)
    {
        // TODO
        *transform = Transform{};
    }

    EXPORT void vr_getHandVelocity(Hand hand, glm::vec3* vel)
    {
        *vel = glm::vec3{ 0.0f };
    }

    EXPORT InputActionHandle vr_getActionHandle(const char* actionPath)
    {
        //return csharpVrInterface->getActionHandle(actionPath);
        return false;
    }

    EXPORT bool vr_getActionHeld(InputActionHandle handle)
    {
        //return csharpVrInterface->getActionHeld(handle);
        return false;
    }

    EXPORT bool vr_getActionPressed(InputActionHandle handle)
    {
        //return csharpVrInterface->getActionPressed(handle);
        return false;
    }

    EXPORT bool vr_getActionReleased(InputActionHandle handle)
    {
        //return csharpVrInterface->getActionReleased(handle);
        return false;
    }

    EXPORT void vr_getActionVector2(InputActionHandle handle, glm::vec2* v2)
    {
        *v2 = glm::vec2{ 0.0f };
    }

    EXPORT void vr_triggerHaptics(InputActionHandle handle, float timeFromNow, float duration, float frequency,
                                  float amplitude)
    {
        //csharpVrInterface->triggerHaptics(handle, timeFromNow, duration, frequency, amplitude);
    }

    EXPORT void vr_getHandBoneTransform(Hand hand, int boneIdx, Transform* transform)
    {
        //*transform = csharpVrInterface->getHandBoneTransform(hand, boneIdx);
    }

    EXPORT bool vr_hasInputFocus()
    {
        return true;
    }
}
