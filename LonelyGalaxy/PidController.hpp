#pragma once
#include <glm/glm.hpp>

namespace lg {
    struct PIDSettings {
        PIDSettings()
            : P { 0.0f }
            , I { 0.0f }
            , D { 0.0f } {}

        PIDSettings(float p, float i, float d)
            : P { p }
            , I { i }
            , D { d } {}

        float P;
        float I;
        float D;
    };

    class V3PidController {
    private:
        glm::vec3 lastError{0.0f};
        glm::vec3 integral{0.0f};

        glm::vec3 clampMagnitude(glm::vec3 in, float maxMagnitude) {
            float inLen = glm::length(in);
            return (in / inLen) * glm::clamp(inLen, -maxMagnitude, maxMagnitude);
        }
    public:
        float P = 0.0f;
        float I = 0.0f;
        float D = 0.0f;

        bool clampIntegral = false;
        float maxIntegralMagnitude = FLT_MAX;

        float averageAmount = 20.0f;

        void acceptSettings(const PIDSettings& settings) {
            P = settings.P;
            I = settings.I;
            D = settings.D;
        }

        glm::vec3 getOutput(glm::vec3 error, float deltaTime, glm::vec3 refVel = glm::vec3{0.0f}) {
            if (glm::any(glm::isnan(lastError))) {
                lastError = glm::vec3{ 0.0f };
            }

            glm::vec3 derivative = ((error - lastError) / deltaTime) + refVel;
            integral += error * deltaTime;

            if (clampIntegral) {
                integral = clampMagnitude(integral, maxIntegralMagnitude);
            }

            if (glm::any(glm::isnan(integral))) {
                integral = glm::vec3(0.0f);
            }

            integral += (error - integral) / averageAmount;

            lastError = error;

            return P * error + I * integral + D * derivative;

        }

        glm::vec3 getOutput(glm::vec3 error, glm::vec3 velocity, float deltaTime) {
            if (glm::any(glm::isnan(lastError))) {
                lastError = glm::vec3{ 0.0f };
            }

            integral += error * deltaTime;

            if (clampIntegral) {
                integral = clampMagnitude(integral, maxIntegralMagnitude);
            }

            if (glm::any(glm::isnan(integral))) {
                integral = glm::vec3(0.0f);
            }

            integral += (error - integral) / averageAmount;

            lastError = error;

            return P * error + I * integral + D * velocity;

        }

        void reset() {
            integral = glm::vec3{ 0.0f };
            lastError = glm::vec3{ 0.0f };
        }
    };

    class StableHandPD {
    private:
        glm::vec3 lastVelocity;

        glm::vec3 clampMagnitude(glm::vec3 in, float maxMagnitude) {
            float inLen = glm::length(in);
            return (in / inLen) * glm::clamp(inLen, -maxMagnitude, maxMagnitude);
        }
    public:
        float P = 0.0f;
        float D = 0.0f;

        void acceptSettings(const PIDSettings& settings) {
            P = settings.P;
            D = settings.D;
        }

        glm::vec3 getOutput(glm::vec3 currPos, glm::vec3 desiredPos, glm::vec3 velocity, float deltaTime, glm::vec3 refVel) {
            glm::vec3 acceleration = velocity - lastVelocity;

            glm::vec3 result = -P * (currPos + (deltaTime * velocity) - desiredPos) -
                D * (velocity + (deltaTime * acceleration) - refVel);

            lastVelocity = velocity;

            return result;
        }

        void reset() {
            lastVelocity = glm::vec3{ 0.0f };
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
