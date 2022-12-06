#pragma once
#include "../Editor.hpp"
#include "AssetCompilation/AssetCompilers.hpp"
#include "Core/AssetDB.hpp"
#include "Editor/AssetEditors.hpp"

namespace worlds
{
    class EntityList : public EditorWindow
    {
    public:
        EntityList(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor)
        {
        }
        void draw(entt::registry& reg) override;
        const char* getName() override
        {
            return "EntityList";
        }
        ~EntityList()
        {
        }
    };

    class Assets : public EditorWindow
    {
    public:
        Assets(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor)
        {
        }
        void draw(entt::registry& reg) override;
        const char* getName() override
        {
            return "Assets";
        }
        ~Assets()
        {
        }
    };

    class RawAssets : public EditorWindow
    {
    public:
        RawAssets(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor)
        {
        }
        void draw(entt::registry& reg) override;
        const char* getName() override
        {
            return "Raw Assets";
        }
        ~RawAssets()
        {
        }
    };

    class GameControls : public EditorWindow
    {
    public:
        GameControls(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor)
        {
            active = false;
        }
        void draw(entt::registry& reg) override;
        const char* getName() override
        {
            return "GameControls";
        }
        ~GameControls()
        {
        }
    };

    class StyleEditor : public EditorWindow
    {
    public:
        StyleEditor(EngineInterfaces interfaces, Editor* editor) : EditorWindow(interfaces, editor)
        {
            active = false;
        }
        void draw(entt::registry& reg) override;
        const char* getName() override
        {
            return "Style Editor";
        }
        ~StyleEditor()
        {
        }
    };

    class MaterialEditorMeta : public EditorWindow
    {
    public:
        MaterialEditorMeta(EngineInterfaces interfaces, Editor* editor);
        void draw(entt::registry& reg) override;
        const char* getName() override
        {
            return "Material Editor";
        }
        ~MaterialEditorMeta();

    private:
        RTTPass* rttPass;
        Camera previewCam;
        entt::entity previewEntity;
    };

    class AboutWindow : public EditorWindow
    {
    public:
        AboutWindow(EngineInterfaces interfaces, Editor* editor) : EditorWindow{interfaces, editor}
        {
            active = false;
        }
        void draw(entt::registry&) override;
        EditorMenu menuSection() override
        {
            return EditorMenu::Help;
        }
        void setActive(bool active) override;
        const char* getName() override
        {
            return "About";
        }
        ~AboutWindow()
        {
        }
    };

    class BakingWindow : public EditorWindow
    {
    public:
        BakingWindow(EngineInterfaces interfaces, Editor* editor) : EditorWindow{interfaces, editor}
        {
            active = false;
        }
        void draw(entt::registry&) override;
        EditorMenu menuSection() override
        {
            return EditorMenu::Edit;
        }
        const char* getName() override
        {
            return "Baking";
        }
        ~BakingWindow()
        {
        }
    };

    class SceneSettingsWindow : public EditorWindow
    {
    public:
        SceneSettingsWindow(EngineInterfaces interfaces, Editor* editor) : EditorWindow{interfaces, editor}
        {
            active = false;
        }
        void draw(entt::registry&) override;
        EditorMenu menuSection() override
        {
            return EditorMenu::Edit;
        }
        const char* getName() override
        {
            return "Scene Settings";
        }
        ~SceneSettingsWindow()
        {
        }
    };

    class AssetEditorWindow : public EditorWindow
    {
    public:
        AssetEditorWindow(AssetID id, EngineInterfaces interfaces, Editor* editor);
        void draw(entt::registry&) override;
        EditorMenu menuSection() override
        {
            return EditorMenu::Edit;
        }
        const char* getName() override
        {
            return "Asset Editor";
        }
        ~AssetEditorWindow();

    private:
        AssetID assetId;
        AssetCompileOperation* currCompileOp;
        IAssetEditor* assetEditor;
    };

    class AssetCompilationManager : public EditorWindow
    {
    public:
        AssetCompilationManager(EngineInterfaces interfaces, Editor* editor) : EditorWindow{interfaces, editor}
        {
            active = false;
        }
        void draw(entt::registry&) override;
        EditorMenu menuSection() override
        {
            return EditorMenu::Edit;
        }
        const char* getName() override
        {
            return "Asset Compilation Manager";
        }
        ~AssetCompilationManager()
        {
        }
    };

    class NodeEditorTest : public EditorWindow
    {
    public:
        NodeEditorTest(EngineInterfaces interfaces, Editor* editor) : EditorWindow{interfaces, editor}
        {
            active = false;
        }
        void draw(entt::registry&) override;
        EditorMenu menuSection() override
        {
            return EditorMenu::Help;
        }
        const char* getName() override
        {
            return "Node Editor Test";
        }
        ~NodeEditorTest()
        {
        }
    };

    class GameView : public EditorWindow
    {
    public:
        GameView(EngineInterfaces interfaces, Editor* editor) : EditorWindow{interfaces, editor}
        {
            active = true;
        }
        void draw(entt::registry&) override;

        EditorMenu menuSection() override
        {
            return EditorMenu::Window;
        }

        const char* getName() override
        {
            return "Game View";
        }

        ~GameView()
        {
        }
    };
}
