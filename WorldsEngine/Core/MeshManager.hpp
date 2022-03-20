#pragma once
#include "Render/Render.hpp"
#include <vector>
#include <robin_hood.h>

namespace worlds {
    struct LoadedMesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        uint8_t numSubmeshes;
        SubmeshInfo submeshes[NUM_SUBMESH_MATS];

        bool skinned;
        std::vector<std::string> boneNames;
        std::vector<glm::mat4> boneRestPositions;
        std::vector<glm::mat4> relativeBoneTransforms;
        float sphereBoundRadius;
    };

    class MeshManager {
    public:
        static const LoadedMesh& get(AssetID id);
        static const LoadedMesh& loadOrGet(AssetID id);
        static void unload(AssetID id);
    private:
        static robin_hood::unordered_node_map<AssetID, LoadedMesh> loadedMeshes;
    };
}
