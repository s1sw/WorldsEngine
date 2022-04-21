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

    void loadToLM(LoadedMesh& lm, AssetID id) {
        std::vector<VertSkinningInfo> vertSkinning;
        LoadedMeshData lmd;
        loadWorldsModel(id, lm.vertices, lm.indices, vertSkinning, lmd);

        lm.numSubmeshes = lmd.numSubmeshes;
        lm.skinned = lmd.isSkinned;
        lm.bones.resize(lmd.meshBones.size());

        for (size_t i = 0; i < lmd.meshBones.size(); i++) {
            Bone& b = lm.bones[i];
            b.id = i;
            b.name = lmd.meshBones[i].name.c_str();
            b.parentId = lmd.meshBones[i].parentIdx;
            b.restPose = lmd.meshBones[i].transform;
        }

        for (int i = 0; i < lmd.numSubmeshes; i++) {
            lm.submeshes[i] = lmd.submeshes[i];
        }

        lm.sphereBoundRadius = 0.0f;
        lm.aabbMax = glm::vec3(0.0f);
        lm.aabbMin = glm::vec3(std::numeric_limits<float>::max());
        for (auto& vtx : lm.vertices) {
            lm.sphereBoundRadius = glm::max(glm::length(vtx.position), lm.sphereBoundRadius);
            lm.aabbMax = glm::max(lm.aabbMax, vtx.position);
            lm.aabbMin = glm::min(lm.aabbMin, vtx.position);
        }
    }

    const LoadedMesh& MeshManager::loadOrGet(AssetID id) {
        if (loadedMeshes.contains(id)) return loadedMeshes.at(id);

        if (!AssetDB::exists(id)) {
            logErr("Mesh ID %u doesn't exist!", id);
            return errorMesh;
        }

        LoadedMesh lm;
        loadToLM(lm, id);

        loadedMeshes.insert({ id, std::move(lm) });

        return loadedMeshes.at(id);
    }

    void MeshManager::unload(AssetID id) {
        loadedMeshes.erase(id);
    }

    void MeshManager::reloadMeshes() {
        // We rely on references to meshes being stable, so
        // rewrite the mesh data in place
        for (auto& pair : loadedMeshes) {
            loadToLM(pair.second, pair.first);
        }
    }
}
