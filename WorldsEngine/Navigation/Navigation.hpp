#pragma once
#include "DetourNavMeshQuery.h"
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>
#include <DetourNavMesh.h>

namespace worlds {
    struct NavigationPath {
        bool valid;
        int numPoints;
        glm::vec3 pathPoints[32];
    };

    class NavigationSystem {
    private:
        static dtNavMesh* navMesh;
        static dtNavMeshQuery* navMeshQuery;
        static void setupFromNavMeshData(uint8_t* data, size_t dataSize);
        static void loadNavMeshFromFile(const char* path);
        static void buildNavMesh(entt::registry& registry, uint8_t*& dataOut, size_t& dataSizeOut);
    public:
        static void buildAndSave(entt::registry& registry, const char* path);
        static void updateNavMesh(entt::registry& registry);
        static void findPath(glm::vec3 startPos, glm::vec3 endPos, NavigationPath& path);
    };
}
