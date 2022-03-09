#include "DebugLines.hpp"
#include <glm/gtx/norm.hpp>
#include <glm/trigonometric.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <vector>
#include <Util/MathsUtil.hpp>

namespace worlds {
    std::vector<DebugLine> buffers[2] = {{}, {}}; 
    int currentBuffer = 0;

    void drawLine(glm::vec3 p0, glm::vec3 p1, glm::vec4 color) {
        buffers[currentBuffer].emplace_back(p0, p1, color);
    }

    void drawCircle(glm::vec3 center, float radius, glm::quat rotation, glm::vec4 color, int detail) {
        int numPoints = detail == 0 ? 32 : detail;
        glm::vec3 prevPoint;

        for (int i = 0; i < numPoints; i++) {
            float angle = glm::pi<float>() * 2.0f * ((float)i / numPoints);

            float xOffset = glm::cos(angle);
            float yOffset = glm::sin(angle);

            glm::vec3 point = center + (rotation * glm::vec3(xOffset * radius, 0.0f, yOffset * radius));

            if (i != 0) {
                drawLine(prevPoint, point, color);
            }

            prevPoint = point;
        }

        drawLine(prevPoint, center + (rotation * glm::vec3(radius, 0.0f, 0.0f)), color);
    }

    const DebugLine* swapDebugLineBuffer(size_t& numLines) {
        numLines = buffers[currentBuffer].size();
        const DebugLine* dbgLines = buffers[currentBuffer].data();

        currentBuffer++;

        if (currentBuffer == 2)
            currentBuffer = 0;

        buffers[currentBuffer].clear();

        return dbgLines;
    }
}
