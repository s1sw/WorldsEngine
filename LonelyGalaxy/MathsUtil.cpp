#include "MathsUtil.hpp"

namespace lg {
    glm::quat fixupQuat(glm::quat q) {
        return q * glm::sign(glm::dot(q, glm::quat_identity<float, glm::packed_highp>()));
    }

    float WrapAngle(float inputAngle) {
        //The inner % 360 restricts everything to +/- 360
        //+360 moves negative values to the positive range, and positive ones to > 360
        //the final % 360 caps everything to 0...360
        return fmodf((fmodf(inputAngle, 360.0f) + 360.0f), 360.0f);
    }

    float AngleToErr(float angle) {
        angle = WrapAngle(angle);
        if (angle > 180.0f && angle < 360.0f) {
            angle = 360.0f - angle;

            angle *= -1.0f;
        }

        return angle;
    }

    glm::quat safeQuatLookat(glm::vec3 dir, glm::vec3 up, glm::vec3 fallbackUp) {
        return glm::abs(glm::dot(dir, up)) > 0.999f ? glm::quatLookAt(dir, fallbackUp) : glm::quatLookAt(dir, up);
    }
}
