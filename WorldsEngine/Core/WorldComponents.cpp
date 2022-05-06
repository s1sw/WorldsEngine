#include "WorldComponents.hpp"
#include "MeshManager.hpp"
#include <entt/entity/entity.hpp>

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

    ChildComponent::ChildComponent() {
        nextChild = entt::null;
        prevChild = entt::null;
    }
}
