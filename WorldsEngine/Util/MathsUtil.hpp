#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace worlds
{
    glm::quat fixupQuat(glm::quat q);
    float WrapAngle(float inputAngle);
    float AngleToErr(float angle);
    glm::quat safeQuatLookat(glm::vec3 dir, glm::vec3 up = {0.0f, 1.0f, 0.0f},
                             glm::vec3 fallbackUp = glm::vec3{0.0f, 0.0f, 1.0f});

    inline glm::vec3 clampMagnitude(glm::vec3 v3, float maxMagnitude)
    {
        float l = glm::length(v3);
        if (l < FLT_EPSILON)
            return glm::vec3{0.0f};
        return glm::normalize(v3) * glm::min(l, maxMagnitude);
    }

    inline void decomposePosRot(const glm::mat4 &mat, glm::vec3 &pos, glm::quat &rot)
    {
        glm::vec3 sc, sk;
        glm::vec4 persp;
        glm::decompose(mat, sc, rot, pos, sk, persp);
    }
}
