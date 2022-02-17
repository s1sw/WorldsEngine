#pragma once
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace worlds {
    struct NavigationPath {
        bool valid;
        int numPoints;
        glm::vec3 pathPoints[32];
    };

    class NavigationSystem {
    public:
        static void updateNavMesh(entt::registry& registry);
        static void findPath(glm::vec3 startPos, glm::vec3 endPos, NavigationPath& path);
    };
}
