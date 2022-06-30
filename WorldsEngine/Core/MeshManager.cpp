#include "MeshManager.hpp"
#include "Util/MathsUtil.hpp"
#include <Render/RenderInternal.hpp>
#include <Render/Loaders/WMDLLoader.hpp>
#include <Core/Log.hpp>

namespace worlds {
    robin_hood::unordered_node_map<AssetID, LoadedMesh> MeshManager::loadedMeshes;
    LoadedMesh errorMesh{ .numSubmeshes = 0 };
    const LoadedMesh& MeshManager::get(AssetID id) {
        return loadedMeshes.at(id);
    }

    void loadToLM(LoadedMesh& lm, AssetID id) {
        LoadedMeshData lmd;

        loadWorldsModel(id, lmd);

        if (lmd.indexType == IndexType::Uint16) {
            lm.indices.resize(lmd.indices16.size());
            for (size_t i = 0; i < lmd.indices16.size(); i++) {
                lm.indices[i] = lmd.indices16[i];
            }
        } else {
            lm.indices = std::move(lmd.indices32);
        }

        lm.vertices = std::move(lmd.vertices);

        lm.numSubmeshes = lmd.submeshes.size();
        lm.skinned = lmd.isSkinned;
        lm.bones.resize(lmd.bones.size());

        for (size_t i = 0; i < lmd.bones.size(); i++) {
            Bone& b = lm.bones[i];
            b.id = i;
            b.name = lmd.bones[i].name.c_str();
            b.parentId = lmd.bones[i].parentIdx;
            b.restPose = lmd.bones[i].transform;
        }

        for (int i = 0; i < lmd.submeshes.size(); i++) {
            const LoadedSubmesh& ls = lmd.submeshes[i];
            lm.submeshes[i].materialIndex = ls.materialIndex;
            lm.submeshes[i].indexOffset = ls.indexOffset;
            lm.submeshes[i].indexCount = ls.indexCount;
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
