#pragma once
#include <glm/fwd.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace worlds {
    void drawLine(glm::vec3 p0, glm::vec3 p1, glm::vec4 color);
    void drawCircle(glm::vec3 center, float radius, glm::quat rotation, glm::vec4 color, int detail = 0);
    void drawSphere(glm::vec3 center, glm::quat rotation, float radius, glm::vec4 color = glm::vec4{1.0f});
    void drawBox(glm::vec3 center, glm::quat rotation, glm::vec3 halfExtents, glm::vec4 color = glm::vec4{1.0f});
    void drawCapsule(glm::vec3 center, glm::quat rotation, float height, float radius, glm::vec4 color = glm::vec4{1.0f});

    struct DebugLine {
        glm::vec3 p0;
        glm::vec3 p1;
        glm::vec4 color;
    };

    // Swaps the buffer used for drawing and return the previously filled buffer
    const DebugLine* swapDebugLineBuffer(size_t& numLines);
}
