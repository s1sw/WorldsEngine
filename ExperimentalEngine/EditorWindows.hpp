#pragma once
#include "Editor.hpp"

namespace worlds {
    class EntityList : public EditorWindow {
    public:
        EntityList(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor) {}
        void draw(entt::registry& reg) override;
        const char* getName() override { return "EntityList"; }
    };

    class Assets : public EditorWindow {
    public:
        Assets(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor) {}
        void draw(entt::registry& reg) override;
        const char* getName() override { return "Assets"; }
    };

    class EntityEditor : public EditorWindow {
    public:
        EntityEditor(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor) {}
        void draw(entt::registry& reg) override;
        const char* getName() override { return "EntityEditor"; }
    };
}