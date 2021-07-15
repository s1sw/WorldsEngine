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

    EXPORT char worldObject_exists(entt::registry* registry, entt::entity entity) {
        return registry->has<WorldObject>(entity);
    }
}
