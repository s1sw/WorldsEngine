#pragma once
#include "../Core/Transform.hpp"
#include <glm/glm.hpp>
#include <string>

namespace worlds {
    enum class VrApi {
        None,
        OpenXR,
        OpenVR
    };

    enum class TrackedObject {
        HMD,
        LeftHand,
        RightHand
    };

    enum class Hand {
        LeftHand,
        RightHand
    };

    typedef uint64_t InputActionHandle;

    class IVRInterface {
    public:
        virtual void updateInput() = 0;
        virtual glm::vec2 getLocomotionInput() = 0;
        virtual bool getSprintInput() = 0;
        virtual bool getJumpInput() = 0;
        virtual glm::mat4 getHeadTransform() = 0;
        virtual bool getHandTransform(Hand hand, Transform& t) = 0;
        virtual InputActionHandle getActionHandle(std::string actionPath) = 0;
        virtual bool getActionHeld(InputActionHandle handle) = 0;
        virtual bool getActionPressed(InputActionHandle handle) = 0;
        virtual bool getActionReleased(InputActionHandle handle) = 0;
        virtual glm::vec2 getActionV2(InputActionHandle handle) = 0;
        virtual ~IVRInterface() {}
    };
}
