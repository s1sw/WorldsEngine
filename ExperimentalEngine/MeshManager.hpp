#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace worlds {
    const uint32_t MAX_SUBMESHES = 32;

    struct SubmeshInfo {
        uint32_t indexOffset;
        uint32_t indexCount;
    };

    struct Mesh {
        uint32_t indexCount;
        SubmeshInfo submeshes[MAX_SUBMESHES];
        uint8_t numSubmeshes;
        float sphereRadius;
        glm::vec3 aabbMin;
        glm::vec3 aabbMax;
    };

    class MeshManager {
    };
}