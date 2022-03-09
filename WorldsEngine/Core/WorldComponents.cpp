#include "WorldComponents.hpp"
#include "MeshManager.hpp"

namespace worlds {
    SkinnedWorldObject::SkinnedWorldObject(AssetID material, AssetID mesh)
        : WorldObject(material, mesh) {
        resetPose();
    }

    void SkinnedWorldObject::resetPose() {
        const LoadedMesh& lm = MeshManager::loadOrGet(mesh);
        currentPose.boneTransforms.clear();

        for (const glm::mat4& t : lm.boneRestPositions) {
            currentPose.boneTransforms.push_back(t);
        }
    }
}
