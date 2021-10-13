#include "MeshManager.hpp"
#include "../Render/RenderInternal.hpp"
#include "../Render/Loaders/WMDLLoader.hpp"

namespace worlds {
    robin_hood::unordered_node_map<AssetID, LoadedMesh> MeshManager::loadedMeshes;
    const LoadedMesh& MeshManager::get(AssetID id) {
        return loadedMeshes.at(id);
    }

    const LoadedMesh& MeshManager::loadOrGet(AssetID id) {
        if (loadedMeshes.contains(id)) return loadedMeshes.at(id);

        std::vector<VertSkinningInfo> vertSkinning;
        LoadedMeshData lmd;
        LoadedMesh lm;
        loadWorldsModel(id, lm.vertices, lm.indices, vertSkinning, lmd);

        lm.numSubmeshes = lmd.numSubmeshes;

        for (int i = 0; i < lmd.numSubmeshes; i++) {
            lm.submeshes[i] = lmd.submeshes[i];
        }

        loadedMeshes.insert({ id, std::move(lm) });

        return loadedMeshes.at(id);
    }
}
