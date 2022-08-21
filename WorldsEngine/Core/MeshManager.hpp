#pragma once
#include "Render/Render.hpp"
#include <robin_hood.h>
#include <slib/String.hpp>
#include <vector>

namespace worlds
{
    struct Bone
    {
        uint32_t id;
        uint32_t parentId;
        glm::mat4 restPose;
        slib::String name;
    };

    struct SubmeshInfo
    {
        uint32_t indexOffset; //!< The offset of the submesh in the mesh index buffer.
        uint32_t indexCount;  //!< The number of indices in the submesh.
        int materialIndex;
        glm::vec3 aabbMax;
        glm::vec3 aabbMin;
    };

    struct LoadedMesh
    {
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

    class MeshManager
    {
    public:
        static const LoadedMesh& get(AssetID id);
        static const LoadedMesh& loadOrGet(AssetID id);
        static void unload(AssetID id);
        static void reloadMeshes();
        static void reloadMesh(AssetID id);

    private:
        static robin_hood::unordered_node_map<AssetID, LoadedMesh> loadedMeshes;
    };
}
