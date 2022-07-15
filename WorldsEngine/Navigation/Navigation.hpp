#pragma once
#include "DetourNavMeshQuery.h"
#include <DetourNavMesh.h>
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace worlds
{
    struct NavigationPath
    {
        bool valid;
        int numPoints;
        glm::vec3 pathPoints[32];
    };

    class NavigationSystem
    {
      private:
        static dtNavMesh *navMesh;
        static dtNavMeshQuery *navMeshQuery;
        static void setupFromNavMeshData(uint8_t *data, size_t dataSize);
        static void loadNavMeshFromFile(const char *path);
        static void buildNavMesh(entt::registry &registry, uint8_t *&dataOut, size_t &dataSizeOut);

      public:
        static void buildAndSave(entt::registry &registry, const char *path);
        static void updateNavMesh(entt::registry &registry);
        static void findPath(glm::vec3 startPos, glm::vec3 endPos, NavigationPath &path);
        static bool getClosestPointOnMesh(glm::vec3 point, glm::vec3 &outPoint,
                                          glm::vec3 searchExtent = glm::vec3{0.0f});
    };
}
