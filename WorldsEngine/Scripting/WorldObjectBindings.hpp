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

    EXPORT uint8_t worldObject_getStaticFlags(entt::registry* registry, entt::entity entity) {
        return (uint8_t)registry->get<WorldObject>(entity).staticFlags;
    }

    EXPORT void worldObject_setStaticFlags(entt::registry* registry, entt::entity entity, uint8_t staticFlags) {
        registry->get<WorldObject>(entity).staticFlags = (StaticFlags)staticFlags;
    }

    EXPORT void worldObject_getUvOffset(entt::registry* registry, entt::entity entity, glm::vec2& offset) {
        glm::vec4 tso = registry->get<WorldObject>(entity).texScaleOffset;
        offset = glm::vec2(tso.z, tso.w);
    }

    EXPORT void worldObject_setUvOffset(entt::registry* registry, entt::entity entity, glm::vec2& offset) {
        WorldObject& wo = registry->get<WorldObject>(entity);
        glm::vec4 tso = wo.texScaleOffset;
        tso.z = offset.x;
        tso.w = offset.y;
        wo.texScaleOffset = tso;
    }

    EXPORT void worldObject_getUvScale(entt::registry* registry, entt::entity entity, glm::vec2& scale) {
        WorldObject& wo = registry->get<WorldObject>(entity);
        glm::vec4 tso = wo.texScaleOffset;
        scale = glm::vec2(tso.x, tso.y);
        wo.texScaleOffset = tso;
    }

    EXPORT void worldObject_setUvScale(entt::registry* registry, entt::entity entity, glm::vec2& scale) {
        WorldObject& wo = registry->get<WorldObject>(entity);
        glm::vec4 tso = wo.texScaleOffset;
        tso.x = scale.x;
        tso.y = scale.y;
    }
}
