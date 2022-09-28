#include "Editor/Editor.hpp"
#include "Editor/GuiUtil.hpp"
#include "Export.hpp"

using namespace worlds;

extern Editor* csharpEditor;
extern "C"
{
    EXPORT uint32_t editor_getCurrentlySelected()
    {
        return (uint32_t)csharpEditor->getSelectedEntity();
    }

    EXPORT void editor_select(uint32_t ent)
    {
        csharpEditor->select((entt::entity)ent);
    }

    EXPORT void editor_addNotification(const char* notification, NotificationType type)
    {
        addNotification(notification, type);
    }

    EXPORT void editor_overrideHandle(entt::entity entity)
    {
        csharpEditor->overrideHandle(entity);
    }

    EXPORT GameState editor_getCurrentState()
    {
        return csharpEditor->getCurrentState();
    }

    EXPORT const slib::List<entt::entity>* editor_getSelectionList()
    {
        return &csharpEditor->getSelectedEntities();
    }
}
