#pragma once
#include <glm/glm.hpp>

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

    class IVRInterface {
    public:
        virtual void updateInput() = 0;
        virtual glm::vec2 getLocomotionInput() = 0;
        virtual bool getSprintInput() = 0;
        virtual bool getJumpInput() = 0;
        virtual glm::mat4 getHeadTransform() = 0;
    };
}