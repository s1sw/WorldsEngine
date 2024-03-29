#include "Export.hpp"
#include "UI/WorldTextComponent.hpp"
#include <entt/entity/registry.hpp>
#include <string.h>

extern "C"
{
    EXPORT void worldtext_setText(entt::registry* registry, entt::entity entity, const char* text)
    {
        registry->get<WorldTextComponent>(entity).setText(text);
    }

    EXPORT uint32_t worldtext_getTextLength(entt::registry* registry, entt::entity entity)
    {
        return (uint32_t)registry->get<WorldTextComponent>(entity).text.size();
    }

    EXPORT void worldtext_getText(entt::registry* registry, entt::entity entity, char* buffer)
    {
        auto& wtc = registry->get<WorldTextComponent>(entity);
        buffer[wtc.text.size()] = 0;
        strncpy(buffer, wtc.text.c_str(), wtc.text.size());
    }

    EXPORT void worldtext_setFont(entt::registry* registry, entt::entity entity, AssetID font)
    {
        registry->get<WorldTextComponent>(entity).setFont(font);
    }

    EXPORT AssetID worldtext_getFont(entt::registry* registry, entt::entity entity, AssetID font)
    {
        return registry->get<WorldTextComponent>(entity).font;
    }

    EXPORT void worldtext_setSize(entt::registry* registry, entt::entity entity, float size)
    {
        registry->get<WorldTextComponent>(entity).textScale = size;
    }

    EXPORT float worldtext_getSize(entt::registry* registry, entt::entity entity)
    {
        return registry->get<WorldTextComponent>(entity).textScale;
    }
}
