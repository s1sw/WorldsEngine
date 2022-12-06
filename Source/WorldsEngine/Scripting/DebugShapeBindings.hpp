#include "Export.hpp"
#include <Render/DebugLines.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace worlds;

extern "C"
{
    EXPORT void debugshapes_drawLine(glm::vec3 p0, glm::vec3 p1, glm::vec4 color)
    {
        drawLine(p0, p1, color);
    }

    EXPORT void debugshapes_drawCircle(glm::vec3 center, float radius, glm::quat rotation, glm::vec4 color)
    {
        drawCircle(center, radius, rotation, color);
    }

    EXPORT void debugshapes_drawSphere(glm::vec3 center, glm::quat rotation, float radius, glm::vec4 color)
    {
        drawSphere(center, rotation, radius, color);
    }

    EXPORT void debugshapes_drawBox(glm::vec3 center, glm::quat rotation, glm::vec3 halfExtents, glm::vec4 color)
    {
        drawBox(center, rotation, halfExtents);
    }
}