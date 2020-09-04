#pragma once
#include <glm/glm.hpp>

namespace converge {
    class V3PidController {
    private:
        glm::vec3 lastError;
        glm::vec3 integral;

        glm::vec3 clampMagnitude(glm::vec3 in, float maxMagnitude) {
            float inLen = glm::length(in);
            return (in / inLen) * glm::clamp(inLen, -maxMagnitude, maxMagnitude);
        }
    public:
        float P = 0.0f;
        float I = 0.0f;
        float D = 0.0f;

        bool clampIntegral;
        float maxIntegralMagnitude;

        float averageAmount = 20.0f;

        glm::vec3 getOutput(glm::vec3 error, float deltaTime) {
            glm::vec3 derivative = (error - lastError) / deltaTime;
            integral += error * deltaTime;

            if (clampIntegral) {
                integral = clampMagnitude(integral, maxIntegralMagnitude);
            }

            integral += (error - integral) / averageAmount;

            lastError = error;

            return P * error + I * integral + D * derivative;
        }

        void reset() {
            integral = glm::vec3{0.0f};
            lastError = glm::vec3{ 0.0f };
        }
    };
}