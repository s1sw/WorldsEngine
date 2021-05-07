#pragma once
#include <unordered_map>
#include <string>
#include <entt/entt.hpp>
#include "ComponentFuncs.hpp"
#include "../Core/Log.hpp"

namespace worlds {
    class ComponentMetadataManager {
    public:
        static std::unordered_map<ENTT_ID_TYPE, ComponentEditor*> metadata;
        static std::vector<ComponentEditor*> sorted;
        static std::unordered_map<ENTT_ID_TYPE, ComponentEditor*> bySerializedID;
        static std::unordered_map<std::string, ComponentEditor*> byName;

        static void setupLookup() {
            ComponentEditorLink* curr = ComponentEditor::first;

            while (curr) {
                metadata.insert({ curr->editor->getComponentID(), curr->editor });
                bySerializedID.insert({ curr->editor->getSerializedID(), curr->editor });
                byName.insert({ curr->editor->getName(), curr->editor });
                logMsg("Found component editor for %s, id is %u", curr->editor->getName(), curr->editor->getComponentID());
                sorted.push_back(curr->editor);
                curr = curr->next;
            }

            std::sort(sorted.begin(), sorted.end(), [](ComponentEditor* a, ComponentEditor* b) {
                return a->getSortID() < b->getSortID();
            });
        }
    };
}
