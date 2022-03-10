#include "DebugLines.hpp"
#include "Core/Transform.hpp"
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

    void drawSphere(glm::vec3 center, glm::quat rotation, float radius, glm::vec4 color) {
        // We can't really draw a sphere with just lines, so draw two circles
        // perpendicular to eachother

        drawCircle(center, radius, rotation, color);

        // Rotate it 90deg on the X axis
        rotation = rotation * glm::angleAxis(glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
        drawCircle(center, radius, rotation, color);
    }

    void drawTransformedLine(const Transform& transform, glm::vec3 p0, glm::vec3 p1, glm::vec4 color) {
        drawLine(transform.transformPoint(p0), transform.transformPoint(p1), color);
    }

    // Draws a box shape using lines.
    void drawBox(glm::vec3 center, glm::quat rotation, glm::vec3 halfExtents, glm::vec4 color) {
        glm::vec3 max = halfExtents;
        glm::vec3 min = -halfExtents;

        Transform overallTransform{center, rotation};

        //                      y
        //      _____           ^
        //     /    /|          |   ^ z
        //max /----/ |          |  /
        //    |    | |          | /
        //    |    | / min      |/
        //    |----|/           ---------> x

        // x->> lines
        drawTransformedLine(overallTransform, glm::vec3(max.x, max.y, max.z), glm::vec3(min.x, max.y, max.z), color);
        drawTransformedLine(overallTransform, glm::vec3(max.x, max.y, min.z), glm::vec3(min.x, max.y, min.z), color);

        drawTransformedLine(overallTransform, glm::vec3(max.x, min.y, max.z), glm::vec3(min.x, min.y, max.z), color);
        drawTransformedLine(overallTransform, glm::vec3(max.x, min.y, min.z), glm::vec3(min.x, min.y, min.z), color);

        // y ^ lines
        drawTransformedLine(overallTransform, glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, max.y, max.z), color);
        drawTransformedLine(overallTransform, glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, max.y, min.z), color);

        drawTransformedLine(overallTransform, glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, max.y, max.z), color);
        drawTransformedLine(overallTransform, glm::vec3(min.x, min.y, min.z), glm::vec3(min.x, max.y, min.z), color);

        // z /^ lines
        drawTransformedLine(overallTransform, glm::vec3(max.x, max.y, max.z), glm::vec3(max.x, max.y, min.z), color);
        drawTransformedLine(overallTransform, glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, min.y, min.z), color);

        drawTransformedLine(overallTransform, glm::vec3(min.x, max.y, max.z), glm::vec3(min.x, max.y, min.z), color);
        drawTransformedLine(overallTransform, glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, min.y, min.z), color);
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
