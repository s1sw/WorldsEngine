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

        for (const Bone& b : lm.bones) {
            currentPose.boneTransforms.push_back(b.restPose);
        }
    }
}
