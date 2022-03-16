#include "EditorActions.hpp"
#include <Core/Log.hpp>
#include <Editor/Editor.hpp>
#include <Util/Fnv.hpp>

namespace worlds {
    robin_hood::unordered_map<SDL_Scancode, EditorActions::KeyBindings> EditorActions::actionBindings;
    robin_hood::unordered_map<uint32_t, EditorAction> EditorActions::registeredActions;

    void EditorActions::addAction(EditorAction&& action) {
        uint32_t idHash = FnvHash(action.id.cStr());

        if (registeredActions.contains(idHash)) {
            logErr("Tried to add an editor action that already exists: %s", action.id.cStr());
            return;
        }

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

    void EditorActions::triggerBoundActions(Editor* ed, entt::registry& reg, SDL_Scancode scancode, ModifierFlags modifiers) {
        if (!actionBindings.contains(scancode)) return;

        KeyBindings& bindings = actionBindings.at(scancode);
        for (int i = 0; i < bindings.numBinds; i++) {
            if (bindings.modifiers[i] == modifiers)
                registeredActions[bindings.actionHashes[i]].function(ed, reg);
        }
    }
}