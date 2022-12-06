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

    EXPORT void vr_getHeadTransform(Transform* transform)
    {
        const UnscaledTransform& ut = csharpVrInterface->getHmdTransform();
        transform->position = ut.position;
        transform->rotation = ut.rotation;
        transform->scale = glm::vec3{ 1.0f };
    }

    EXPORT uint64_t vr_getActionHandle(const char* actionSet, const char* action)
    {
        return csharpVrInterface->getActionHandle(actionSet, action);
    }

    EXPORT uint64_t vr_getSubactionHandle(const char* subaction)
    {
        return csharpVrInterface->getSubactionHandle(subaction);
    }

    EXPORT BooleanActionState vr_getBooleanActionState(uint64_t actionHandle, uint64_t subactionHandle)
    {
        return csharpVrInterface->getBooleanActionState(actionHandle, subactionHandle);
    }

    EXPORT FloatActionState vr_getFloatActionState(uint64_t actionHandle, uint64_t subactionHandle)
    {
        return csharpVrInterface->getFloatActionState(actionHandle, subactionHandle);
    }

    EXPORT Vector2fActionState vr_getVector2fActionState(uint64_t actionHandle, uint64_t subactionHandle)
    {
        return csharpVrInterface->getVector2fActionState(actionHandle, subactionHandle);
    }

    EXPORT void vr_getPoseActionState(uint64_t actionHandle, uint64_t subactionHandle, Transform* t)
    {
        auto us = csharpVrInterface->getPoseActionState(actionHandle, subactionHandle);
        t->position = us.position;
        t->rotation = us.rotation;
        t->scale = glm::vec3{ 1.0f };
    }

    EXPORT void vr_triggerHaptics(float duration, float frequency, float amplitude, uint64_t actionHandle, uint64_t subactionHandle)
    {
        csharpVrInterface->applyHapticFeedback(duration, frequency, amplitude, actionHandle, subactionHandle);
    }

    EXPORT bool vr_hasInputFocus()
    {
        return true;
    }
}
