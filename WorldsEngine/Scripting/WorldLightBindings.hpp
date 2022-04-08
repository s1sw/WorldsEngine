#include "Export.hpp"
#include "Core/Engine.hpp"

using namespace worlds;

extern "C" {
    EXPORT bool worldlight_getEnabled(entt::registry* registry, entt::entity entity) {
        return registry->get<WorldLight>(entity).enabled;
    }

    EXPORT void worldlight_setEnabled(entt::registry* registry, entt::entity entity, bool enabled) {
        registry->get<WorldLight>(entity).enabled = enabled;
    }

    EXPORT float worldlight_getIntensity(entt::registry* registry, entt::entity entity) {
        return registry->get<WorldLight>(entity).intensity;
    }

    EXPORT void worldlight_setIntensity(entt::registry* registry, entt::entity entity, float intensity) {
        registry->get<WorldLight>(entity).intensity = intensity;
    }

    EXPORT glm::vec3 worldlight_getColor(entt::registry* registry, entt::entity entity) {
        return registry->get<WorldLight>(entity).color;
    }

    EXPORT void worldlight_setColor(entt::registry* registry, entt::entity entity, glm::vec3 color) {
        registry->get<WorldLight>(entity).color = color;
    }

    EXPORT float worldlight_getRadius(entt::registry* registry, entt::entity entity) {
        return registry->get<WorldLight>(entity).maxDistance;
    }

    EXPORT void worldlight_setRadius(entt::registry* registry, entt::entity entity, float radius) {
        registry->get<WorldLight>(entity).maxDistance = radius;
    }

    EXPORT LightType worldlight_getType(entt::registry* registry, entt::entity entity) {
        return registry->get<WorldLight>(entity).type;
    }

    EXPORT void worldlight_setType(entt::registry* registry, entt::entity entity, LightType type) {
        registry->get<WorldLight>(entity).type = type;
    }
}
