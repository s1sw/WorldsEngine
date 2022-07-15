#pragma once
#include "entt/entity/lw_fwd.hpp"
#include "robin_hood.h"
#include "slib/String.hpp"
#include <SDL_scancode.h>
#include <cstdint>
#include <entt/entity/lw_fwd.hpp>
#include <slib/List.hpp>
#include <slib/String.hpp>
#include <string>

namespace worlds
{
    class Editor;
    enum class ModifierFlags
    {
        None = 0,
        Shift = 1,
        Control = 2,
        Alt = 4
    };

    inline ModifierFlags operator|(ModifierFlags lhs, ModifierFlags rhs)
    {
        return static_cast<ModifierFlags>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
    }

    struct ActionKeybind
    {
        SDL_Scancode key;
        ModifierFlags flags;
    };

    struct EditorAction
    {
        slib::String id;
        std::function<void(Editor* ed, entt::registry& reg)> function;
        slib::String friendlyString;
    };

    class EditorActions
    {
      public:
        static void addAction(EditorAction&& action);
        static const EditorAction& findAction(const char* id);
        static void bindAction(const char* id, ActionKeybind keybind);
        static void disableForThisFrame();
        static void reenable();
        static void triggerBoundActions(Editor* ed, entt::registry& reg, SDL_Scancode scancode,
                                        ModifierFlags modifiers);
        static const EditorAction& getActionByHash(uint32_t hash);
        static slib::List<uint32_t> searchForActions(slib::String pattern);

      private:
        struct KeyBindings
        {
            ModifierFlags modifiers[4];
            uint32_t actionHashes[4];
            uint8_t numBinds;
        };

        static robin_hood::unordered_map<SDL_Scancode, KeyBindings> actionBindings;
        static robin_hood::unordered_node_map<uint32_t, EditorAction> registeredActions;
        static slib::List<EditorAction> actionList;
    };
}