#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace converge {
    glm::quat fixupQuat(glm::quat q);
    float WrapAngle(float inputAngle);
    float AngleToErr(float angle);
}
