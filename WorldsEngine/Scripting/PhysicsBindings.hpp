#include "Export.hpp"
#include "Physics/Physics.hpp"

using namespace worlds;

extern "C" {
    EXPORT uint32_t physics_raycast(glm::vec3 origin, glm::vec3 direction, float maxDist, RaycastHitInfo* hitInfo) {
        return (uint32_t)raycast(origin, direction, maxDist, hitInfo);
    }
}
