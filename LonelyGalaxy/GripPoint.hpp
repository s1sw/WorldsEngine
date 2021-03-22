#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lg {
    struct GripPoint {
        glm::vec3 offset{0.0f};
        glm::quat rotOffset{};
        bool exclusive = true;
        bool currentlyHeld = false;
    };
}
