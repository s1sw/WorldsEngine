#include "Export.hpp"
#include "Core/Engine.hpp"

using namespace worlds;

extern "C" {
    EXPORT uint32_t worldObject_getMesh(entt::registry* registry, entt::entity entity) {
        WorldObject& wo = registry->get<WorldObject>(entity);

        return wo.mesh;
    }

    EXPORT void worldObject_setMesh(entt::registry* registry, entt::entity entity, AssetID id) {
        WorldObject& wo = registry->get<WorldObject>(entity);

        wo.mesh = id;
    }

    EXPORT uint32_t worldObject_getMaterial(entt::registry* registry, entt::entity entity, uint32_t materialIndex) {
        return registry->get<WorldObject>(entity).materials[materialIndex];
    }

    EXPORT void worldObject_setMaterial(entt::registry* registry, entt::entity entity, uint32_t materialIndex, AssetID material) {
        registry->get<WorldObject>(entity).materials[materialIndex] = material;
    }

    EXPORT char worldObject_exists(entt::registry* registry, entt::entity entity) {
        return registry->has<WorldObject>(entity);
    }
}
