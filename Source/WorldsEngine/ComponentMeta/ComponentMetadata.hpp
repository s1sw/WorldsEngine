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
        static std::unordered_map<ENTT_ID_TYPE, ComponentMetadata*> metadata;
        static std::vector<ComponentMetadata*> sorted;
        static std::unordered_map<ENTT_ID_TYPE, ComponentMetadata*> bySerializedID;
        static std::unordered_map<std::string, ComponentMetadata*> byName;

        static void setupLookup(EngineInterfaces* interfaces)
        {
            ComponentEditorLink* curr = ComponentMetadata::first;

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
                      [](ComponentMetadata* a, ComponentMetadata* b) { return a->getSortID() < b->getSortID(); });
        }
    };
}
