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

        static void setupLookup() {
            ComponentEditorLink* curr = ComponentEditor::first;

            while (curr) {
                metadata.insert({ curr->editor->getComponentID(), curr->editor });
                bySerializedID.insert({ curr->editor->getSerializedID(), curr->editor });
                logMsg("Found component editor for %s", curr->editor->getName());
                sorted.push_back(curr->editor);
                curr = curr->next;
            }

            std::sort(sorted.begin(), sorted.end(), [](ComponentEditor* a, ComponentEditor* b) {
                return a->getSortID() < b->getSortID();
            });
        }
    };
}
