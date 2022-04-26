#pragma once
#include "Render/Render.hpp"
#include <vector>
#include <robin_hood.h>
#include <slib/String.hpp>

namespace worlds {
    struct Bone {
        uint32_t id;
        uint32_t parentId;
        glm::mat4 restPose;
        slib::String name;
    };

    struct LoadedMesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        uint8_t numSubmeshes;
        SubmeshInfo submeshes[NUM_SUBMESH_MATS];

        bool skinned;
        std::vector<Bone> bones;
        float sphereBoundRadius;
        glm::vec3 aabbMin;
        glm::vec3 aabbMax;
    };

    class MeshManager {
    public:
        static const LoadedMesh& get(AssetID id);
        static const LoadedMesh& loadOrGet(AssetID id);
        static void unload(AssetID id);
        static void reloadMeshes();
    private:
        static robin_hood::unordered_node_map<AssetID, LoadedMesh> loadedMeshes;
    };
}
