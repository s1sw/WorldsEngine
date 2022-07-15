#pragma once
#include "../Core/Log.hpp"
#include "ComponentFuncs.hpp"
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>

namespace worlds
{
    struct EngineInterfaces;

    class ComponentMetadataManager
    {
      public:
        static std::unordered_map<ENTT_ID_TYPE, ComponentEditor*> metadata;
        static std::vector<ComponentEditor*> sorted;
        static std::unordered_map<ENTT_ID_TYPE, ComponentEditor*> bySerializedID;
        static std::unordered_map<std::string, ComponentEditor*> byName;

        static void setupLookup(EngineInterfaces* interfaces)
        {
            ComponentEditorLink* curr = ComponentEditor::first;

            while (curr)
            {
                metadata.insert({curr->editor->getComponentID(), curr->editor});
                bySerializedID.insert({curr->editor->getSerializedID(), curr->editor});
                byName.insert({curr->editor->getName(), curr->editor});
                sorted.push_back(curr->editor);
                curr->editor->setInterfaces(interfaces);
                curr = curr->next;
            }

            std::sort(sorted.begin(), sorted.end(),
                      [](ComponentEditor* a, ComponentEditor* b) { return a->getSortID() < b->getSortID(); });
        }
    };
}
