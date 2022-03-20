#include "MeshManager.hpp"
#include "Util/MathsUtil.hpp"
#include <Render/RenderInternal.hpp>
#include <Render/Loaders/WMDLLoader.hpp>

namespace worlds {
    robin_hood::unordered_node_map<AssetID, LoadedMesh> MeshManager::loadedMeshes;
    LoadedMesh errorMesh{ .numSubmeshes = 0 };
    const LoadedMesh& MeshManager::get(AssetID id) {
        return loadedMeshes.at(id);
    }

    const LoadedMesh& MeshManager::loadOrGet(AssetID id) {
        if (loadedMeshes.contains(id)) return loadedMeshes.at(id);

        if (!AssetDB::exists(id)) {
            logErr("Failed to load mesh ID %u", id);
            return errorMesh;
        }
        std::vector<VertSkinningInfo> vertSkinning;
        LoadedMeshData lmd;
        LoadedMesh lm;
        loadWorldsModel(id, lm.vertices, lm.indices, vertSkinning, lmd);

        lm.numSubmeshes = lmd.numSubmeshes;
        lm.skinned = lmd.isSkinned;
        lm.boneNames.resize(lmd.meshBones.size());
        lm.boneRestPositions.resize(lmd.meshBones.size());
        lm.relativeBoneTransforms.resize(lmd.meshBones.size());

        for (size_t i = 0; i < lm.boneNames.size(); i++) {
            lm.boneNames[i] = lmd.meshBones[i].name;
            lm.boneRestPositions[i] = lmd.meshBones[i].transform;
            glm::vec3 p;
            glm::quat r;
            decomposePosRot(lm.boneRestPositions[i], p, r);
            lm.relativeBoneTransforms[i] = lmd.meshBones[i].transform;
        }

        for (int i = 0; i < lmd.numSubmeshes; i++) {
            lm.submeshes[i] = lmd.submeshes[i];
        }

        lm.sphereBoundRadius = 0.0f;
        for (auto& vtx : lm.vertices) {
            lm.sphereBoundRadius = glm::max(glm::length(vtx.position), lm.sphereBoundRadius);
        }

        loadedMeshes.insert({ id, std::move(lm) });

        return loadedMeshes.at(id);
    }

    void MeshManager::unload(AssetID id) {
        loadedMeshes.erase(id);
    }
}
