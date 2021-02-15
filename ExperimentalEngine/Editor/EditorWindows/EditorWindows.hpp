#pragma once
#include "../Editor.hpp"

namespace worlds {
    class EntityList : public EditorWindow {
    public:
        EntityList(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor) {}
        void draw(entt::registry& reg) override;
        const char* getName() override { return "EntityList"; }
        ~EntityList() {}
    };

    class Assets : public EditorWindow {
    public:
        Assets(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor) {}
        void draw(entt::registry& reg) override;
        const char* getName() override { return "Assets"; }
        ~Assets() {}
    };

    class EntityEditor : public EditorWindow {
    public:
        EntityEditor(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor) {}
        void draw(entt::registry& reg) override;
        const char* getName() override { return "EntityEditor"; }
        ~EntityEditor() {}
    };

    class GameControls : public EditorWindow {
    public:
        GameControls(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor) { active = false; }
        void draw(entt::registry& reg) override;
        const char* getName() override { return "GameControls"; }
        ~GameControls() {}
    };

    class StyleEditor : public EditorWindow {
    public:
        StyleEditor(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor) { active = false; }
        void draw(entt::registry& reg) override;
        const char* getName() override { return "Style Editor"; }
        ~StyleEditor() {}
    };

    class AssetDBExplorer : public EditorWindow {
    public:
        AssetDBExplorer(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor) { active = false; }
        void draw(entt::registry& reg) override;
        const char* getName() override { return "AssetDB Explorer"; }
        ~AssetDBExplorer() {}
    };

    class MaterialEditor : public EditorWindow {
    public:
        MaterialEditor(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor) { active = false; }
        void draw(entt::registry& reg) override;
        const char* getName() override { return "Material Editor"; }
        ~MaterialEditor() {}
    };
}
