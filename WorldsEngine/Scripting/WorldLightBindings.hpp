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
}
