#include "Export.hpp"
#include "Core/Engine.hpp"
#include <Core/Transform.hpp>

using namespace worlds;

extern "C" {
    EXPORT uint32_t skinnedWorldObject_getMesh(entt::registry* registry, entt::entity entity) {
        SkinnedWorldObject& wo = registry->get<SkinnedWorldObject>(entity);

        return wo.mesh;
    }

    EXPORT void skinnedWorldObject_setMesh(entt::registry* registry, entt::entity entity, AssetID id) {
        SkinnedWorldObject& wo = registry->get<SkinnedWorldObject>(entity);

        wo.mesh = id;
    }

    EXPORT uint32_t skinnedWorldObject_getMaterial(entt::registry* registry, entt::entity entity, uint32_t materialIndex) {
        return registry->get<SkinnedWorldObject>(entity).materials[materialIndex];
    }

    EXPORT void skinnedWorldObject_setMaterial(entt::registry* registry, entt::entity entity, uint32_t materialIndex, AssetID material) {
        registry->get<SkinnedWorldObject>(entity).materials[materialIndex] = material;
    }

    EXPORT char skinnedWorldObject_exists(entt::registry* registry, entt::entity entity) {
        return registry->has<SkinnedWorldObject>(entity);
    }

    EXPORT void skinnedWorldObject_getBoneTransform(entt::registry* registry, entt::entity entity, uint32_t idx, Transform* t) {
        auto& swo = registry->get<SkinnedWorldObject>(entity);
        t->fromMatrix(swo.currentPose.boneTransforms[idx]);
    }

    EXPORT void skinnedWorldObject_setBoneTransform(entt::registry* registry, entt::entity entity, uint32_t idx, Transform* t) {
        auto& swo = registry->get<SkinnedWorldObject>(entity);
        swo.currentPose.boneTransforms[idx] = t->getMatrix();
    }
}
