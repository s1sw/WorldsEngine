#include "ComponentMeta/ComponentMetadata.hpp"
#include "Export.hpp"

using namespace worlds;

Editor* csharpEditor;
extern "C"
{
    EXPORT int componentmeta_getDataCount()
    {
        return ComponentMetadataManager::sorted.size();
    }

    EXPORT const char* componentmeta_getName(int index)
    {
        return ComponentMetadataManager::sorted[index]->getName();
    }

#ifdef BUILD_EDITOR
    EXPORT void componentmeta_edit(entt::registry* reg, entt::entity entity, int index)
    {
        ComponentMetadata* meta = ComponentMetadataManager::sorted[index];
        meta->edit(entity, *reg, csharpEditor);
    }

    EXPORT void componentmeta_drawGizmos(entt::registry* reg, entt::entity entity, int index)
    {
        ComponentMetadata* meta = ComponentMetadataManager::sorted[index];
        meta->drawGizmos(entity, *reg, csharpEditor);
    }
#endif

    EXPORT void componentmeta_create(entt::registry* reg, entt::entity entity, int index)
    {
        ComponentMetadata* meta = ComponentMetadataManager::sorted[index];

        meta->create(entity, *reg);
    }

    EXPORT void componentmeta_destroy(entt::registry* reg, entt::entity entity, int index)
    {
        ComponentMetadata* meta = ComponentMetadataManager::sorted[index];

        meta->destroy(entity, *reg);
    }

    EXPORT void componentmeta_clone(entt::registry* reg, entt::entity from, entt::entity to, int index)
    {
        ComponentMetadata* meta = ComponentMetadataManager::sorted[index];

        meta->clone(from, to, *reg);
    }

    EXPORT bool componentmeta_hasComponent(entt::registry* reg, entt::entity entity, int index)
    {
        ComponentMetadata* meta = ComponentMetadataManager::sorted[index];

        ENTT_ID_TYPE t[] = {meta->getComponentID()};
        auto rtView = reg->runtime_view(std::cbegin(t), std::cend(t));

        return rtView.contains(entity);
    }

    EXPORT bool componentmeta_allowInspectorAdd(int index)
    {
        return ComponentMetadataManager::sorted[index]->allowInspectorAdd();
    }
}
