#include "EditorActions.hpp"
#include <Core/Log.hpp>
#include <Editor/Editor.hpp>
#include <Util/Fnv.hpp>
#include <algorithm>
#define FTS_FUZZY_MATCH_IMPLEMENTATION
#include <fts_fuzzy_match.h>

namespace worlds {
    robin_hood::unordered_map<SDL_Scancode, EditorActions::KeyBindings> EditorActions::actionBindings;
    robin_hood::unordered_map<uint32_t, EditorAction> EditorActions::registeredActions;
    slib::List<EditorAction> EditorActions::actionList;

    void EditorActions::addAction(EditorAction&& action) {
        uint32_t idHash = FnvHash(action.id.cStr());

        if (registeredActions.contains(idHash)) {
            logErr("Tried to add an editor action that already exists: %s", action.id.cStr());
            return;
        }

        if (!action.friendlyString.empty())
            actionList.add(action);

        registeredActions.insert({ idHash, std::move(action) });
    }

    const EditorAction& EditorActions::findAction(const char* id) {
        uint32_t idHash = FnvHash(id);

        return registeredActions.at(idHash);
    }

    void EditorActions::bindAction(const char *id, ActionKeybind keybind) {
        if (!actionBindings.contains(keybind.key)) {
            // Insert an empty KeyBindings object for the scancode
            actionBindings.insert({keybind.key, KeyBindings{{}, {}, 0}});
        }
        
        KeyBindings& keyBindings = actionBindings.at(keybind.key);

        int idx = keyBindings.numBinds++;
        keyBindings.modifiers[idx] = keybind.flags;
        keyBindings.actionHashes[idx] = FnvHash(id);
    }

    bool disabled = false;

    void EditorActions::disableForThisFrame() {
        disabled = true;
    }

    void EditorActions::reenable() {
        disabled = false;
    }

    void EditorActions::triggerBoundActions(Editor* ed, entt::registry& reg, SDL_Scancode scancode, ModifierFlags modifiers) {
        if (disabled || !actionBindings.contains(scancode)) return;

        KeyBindings& bindings = actionBindings.at(scancode);
        for (int i = 0; i < bindings.numBinds; i++) {
            if (bindings.modifiers[i] == modifiers)
                registeredActions[bindings.actionHashes[i]].function(ed, reg);
        }
    }

    const EditorAction& EditorActions::getActionByHash(uint32_t hash) {
        return registeredActions.at(hash);
    }

    struct SearchCandidate {
        uint32_t actionHash;
        int score;
    };

    slib::List<uint32_t> EditorActions::searchForActions(slib::String pattern) {
        std::vector<SearchCandidate> candidates;

        for (EditorAction& action : actionList) {
            int score;
            if (fts::fuzzy_match(pattern.cStr(), action.friendlyString.cStr(), score)) {
                candidates.push_back({ FnvHash(action.id.cStr()), score });
            }
        }

        std::sort(candidates.begin(), candidates.end(), [](const SearchCandidate& a, const SearchCandidate& b) {
            return a.score < b.score;
        });

        slib::List<uint32_t> orderedHashes;
        orderedHashes.reserve(candidates.size());

        for (SearchCandidate& sc : candidates) {
            orderedHashes.add(sc.actionHash);
        }

        return orderedHashes;
    }
}