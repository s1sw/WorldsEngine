#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace converge {
    glm::quat fixupQuat(glm::quat q);
    float WrapAngle(float inputAngle);
    float AngleToErr(float angle);
    glm::quat safeQuatLookat(glm::vec3 dir, 
                             glm::vec3 up = {0.0f, 1.0f, 0.0f}, 
                             glm::vec3 fallbackUp = glm::vec3 {0.0f, 0.0f, 1.0f});
}
