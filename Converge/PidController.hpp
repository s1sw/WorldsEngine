#pragma once
#include <glm/glm.hpp>

namespace converge {
    struct PIDSettings {
        PIDSettings()
            : P { 0.0f }
            , I { 0.0f }
            , D { 0.0f } {}
        float P;
        float I;
        float D;
    };

    class V3PidController {
    private:
        glm::vec3 lastError;
        glm::vec3 lastDerivative;
        glm::vec3 integral;

        glm::vec3 clampMagnitude(glm::vec3 in, float maxMagnitude) {
            float inLen = glm::length(in);
            return (in / inLen) * glm::clamp(inLen, -maxMagnitude, maxMagnitude);
        }
    public:
        float P = 0.0f;
        float I = 0.0f;
        float D = 0.0f;
        float D2 = 0.0f;

        bool clampIntegral;
        float maxIntegralMagnitude;

        float averageAmount = 20.0f;

        void acceptSettings(const PIDSettings& settings) {
            P = settings.P;
            I = settings.I;
            D = settings.D;
        }

        glm::vec3 getOutput(glm::vec3 error, float deltaTime) {
            glm::vec3 derivative = (error - lastError) / deltaTime;
            glm::vec3 derivative2 = ((derivative * deltaTime) - (lastDerivative * deltaTime)) / deltaTime;
            integral += error * deltaTime;

            if (clampIntegral) {
                integral = clampMagnitude(integral, maxIntegralMagnitude);
            }

            if (glm::any(glm::isnan(integral))) {
                integral = glm::vec3(0.0f);
            }

            integral += (error - integral) / averageAmount;

            lastError = error;
            lastDerivative = derivative;

            return P * error + I * integral + D * derivative + D2 * derivative2;
        }

        void reset() {
            integral = glm::vec3{0.0f};
            lastError = glm::vec3{ 0.0f };
        }
    };

    class PidController {
    private:
        float lastError;
        float integral;

    public:
        float P = 0.0f;
        float I = 0.0f;
        float D = 0.0f;
        float D2 = 0.0f;

        bool clampIntegral;
        float maxIntegralMagnitude;

        float averageAmount = 20.0f;

        float getOutput(float error, float deltaTime) {
            float derivative = (error - lastError) / deltaTime;
            integral += error * deltaTime;

            if (clampIntegral) {
                integral = glm::clamp(integral, -maxIntegralMagnitude, maxIntegralMagnitude);
            }

            if (glm::isnan(integral)) {
                integral = 0.0f;
            }

            integral += (error - integral) / averageAmount;

            lastError = error;

            return P * error + I * integral + D * derivative;
        }

        void reset() {
            integral = 0.0f;
            lastError = 0.0f;
        }
    };
}
