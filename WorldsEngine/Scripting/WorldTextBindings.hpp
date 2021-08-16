#include "Export.hpp"
#include "UI/WorldTextComponent.hpp"

namespace worlds {
    EXPORT void worldtext_setText(entt::registry* registry, entt::entity entity, const char* text) {
        registry->get<WorldTextComponent>(entity).setText(text);
    }

    EXPORT uint32_t worldtext_getTextLength(entt::registry* registry, entt::entity entity) {
        return (uint32_t)registry->get<WorldTextComponent>(entity).text.size();
    }

    EXPORT void worldtext_getText(entt::registry* registry, entt::entity entity, char* buffer) {
        auto& wtc = registry->get<WorldTextComponent>(entity);
        buffer[wtc.name.size()] = 0;
        strncpy(buffer, wtc.name.c_str(), wtc.name.size());
    }

    EXPORT void worldtext_setFont(entt::registry* registry, entt::entity entity, AssetID font) {
        registry->get<WorldTextComponent>(entity).setFont(font);
    }

    EXPORT AssetID worldtext_getFont(entt::registry* registry, entt::entity entity, AssetID font) {
        return registry->get<WorldTextComponent>(entity).font;
    }
}
