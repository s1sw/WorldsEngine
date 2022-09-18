#pragma once
#include "AssetCompilation/AssetCompilers.hpp"
#include "EditorActionSearchPopup.hpp"
#include "EditorAssetSearchPopup.hpp"
#include <Core/IGameEventHandler.hpp>
#include <Core/Transform.hpp>
#include <Editor/EditorActions.hpp>
#include <ImGui/imgui.h>
#include <Input/Input.hpp>
#include <Render/Camera.hpp>
#include <deque>
#include <entt/entt.hpp>
#include <memory>
#include <slib/List.hpp>
#include <string>
#include <thread>

namespace worlds
{
    typedef uint32_t AssetID;
    class RTTPass;

    enum class Tool
    {
        None = 0,
        Translate = 1,
        Rotate = 2,
        Scale = 4,
        Bounds = 8
    };

    enum class AxisFlagBits
    {
        None = 0,
        X = 1,
        Y = 2,
        Z = 4,
        All = 7
    };

    inline AxisFlagBits operator|(AxisFlagBits lhs, AxisFlagBits rhs)
    {
        return static_cast<AxisFlagBits>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
    }

    inline AxisFlagBits operator&(AxisFlagBits lhs, AxisFlagBits rhs)
    {
        return static_cast<AxisFlagBits>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs));
    }

    inline AxisFlagBits operator^(AxisFlagBits lhs, AxisFlagBits rhs)
    {
        return static_cast<AxisFlagBits>(static_cast<unsigned>(lhs) ^ static_cast<unsigned>(rhs));
    }

    inline Tool operator|(Tool lhs, Tool rhs)
    {
        return static_cast<Tool>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
    }

    inline Tool operator&(Tool lhs, Tool rhs)
    {
        return static_cast<Tool>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs));
    }

    struct EditorSettings
    {
        bool objectSnapGlobal = false;
        float snapIncrement = 0.1f;
        float angularSnapIncrement = 15.0f;
        float sceneIconDrawDistance = 10.0f;
        bool enableShadows = true;
    };

    class Editor;

    enum class EditorMenu
    {
        File,
        Edit,
        Window,
        Help
    };

    struct AssetFile
    {
        AssetID sourceAssetId;
        std::string path;
        int64_t lastModified;
        std::string compiledPath;
        bool needsCompile = false;
        bool isCompiled = false;
        bool dependenciesExist = true;
    };

    class GameProject;
    class ProjectAssetCompiler;

    class ProjectAssets
    {
    public:
        ProjectAssets(const GameProject& project);
        ~ProjectAssets();
        void startWatcherThread();
        std::vector<AssetFile> assetFiles;
        void checkForAssetChanges();
        void checkForAssetChange(AssetFile& file);
        void enumerateAssets();
        slib::List<AssetID> searchForAssets(slib::String pattern);
        bool recompileFlag = false;

    private:
        void enumerateForAssets(const char* path);
        volatile bool threadActive;
        const GameProject& project;
        std::thread watcherThread;
    };

    class GameProject
    {
      public:
        GameProject(std::string path);
        std::string_view name() const;
        std::string_view root() const;
        std::string_view sourceData() const;
        std::string_view builtData() const;
        std::string_view rawData() const;
        ProjectAssets& assets();
        ProjectAssetCompiler& assetCompiler();
        void mountPaths();
        void unmountPaths();

      private:
        std::string _name;
        std::string _root;
        std::string _srcDataPath;
        std::string _compiledDataPath;
        std::string _rawPath;
        std::vector<std::string> _copyDirs;
        std::unique_ptr<ProjectAssets> _projectAssets;
        std::unique_ptr<ProjectAssetCompiler> _assetCompiler;
    };

    class EditorWindow
    {
      public:
        EditorWindow(EngineInterfaces interfaces, Editor* editor) : interfaces(interfaces), editor(editor), active(true)
        {
        }

        virtual bool isActive()
        {
            return active;
        }
        virtual void setActive(bool active)
        {
            this->active = active;
        }
        virtual void draw(entt::registry& reg) = 0;
        virtual EditorMenu menuSection()
        {
            return EditorMenu::Window;
        }
        virtual const char* getName() = 0;
        virtual ~EditorWindow(){};

      protected:
        EngineInterfaces interfaces;
        Editor* editor;
        bool active;
    };

    class EditorUndo
    {
      public:
        void pushState(entt::registry& reg);
        void undo(entt::registry& reg);
        void redo(entt::registry& reg);
        void clear();
        uint32_t modificationCount()
        {
            return currentPos;
        }

      private:
        uint32_t highestSaved = 0;
        uint32_t currentPos = 0;
    };

    class EditorSceneView
    {
      public:
        EditorSceneView(EngineInterfaces interfaces, Editor* ed);
        void drawWindow(int unqiueId);
        void recreateRTT();
        void setShadowsEnabled(bool enabled);
        void setViewportActive(bool active);
        Camera& getCamera();
        bool open = true;
        bool isSeparateWindow = false;
        ~EditorSceneView();

      private:
        void updateCamera(float deltaTime);
        uint32_t currentWidth, currentHeight;
        RTTPass* sceneViewPass = nullptr;
        Camera cam;
        EngineInterfaces interfaces;
        Editor* ed;
        float lookX = 0.0f;
        float lookY = 0.0f;
        bool shadowsEnabled = true;
        bool viewportActive = true;
        float drawerAnimationProgress = 0.0f;
        ImTextureID audioSourceIcon;
        ImTextureID worldLightIcon;
        ImTextureID worldCubemapIcon;
    };

    struct EntityFolder
    {
        EntityFolder(std::string name);
        std::string name;
        uint32_t randomId;
        std::vector<EntityFolder> children;
        std::vector<entt::entity> entities;
    };

    struct EntityFolders
    {
        EntityFolder rootFolder{"Root"};
    };

    class AssetEditorWindow;

    enum class GameState : uint8_t
    {
        Editing,
        Playing,
        Paused
    };

    class Editor
    {
      public:
        Editor(entt::registry& reg, EngineInterfaces& interfaces);
        ~Editor();
        void select(entt::entity entity);
        void multiSelect(entt::entity entity);
        void update(float deltaTime);
        void activateTool(Tool newTool);
        entt::entity getSelectedEntity()
        {
            return currentSelectedEntity;
        }
        const slib::List<entt::entity>& getSelectedEntities() const
        {
            return selectedEntities;
        }
        bool isEntitySelected(entt::entity ent) const;
        EditorUndo undo;
        void overrideHandle(Transform* t);
        void overrideHandle(entt::entity entity);
        bool entityEyedropper(entt::entity& picked);

        void openProject(std::string projectPath);
        GameProject& currentProject()
        {
            return *project;
        }

        void saveOpenWindows();
        void loadOpenWindows();
        EditorSceneView* getFirstSceneView();
        void openAsset(AssetID id);
        GameState getCurrentState()
        {
            return currentState;
        }

        bool isPlaying()
        {
            return currentState == GameState::Playing;
        }

      private:
        EditorActionSearchPopup actionSearch;
        EditorAssetSearchPopup assetSearch;
        std::unique_ptr<GameProject> project;
        ImTextureID titleBarIcon;
        void handleTools(Transform& t, ImVec2 wPos, ImVec2 wSize, Camera& camera);
        std::string generateWindowTitle();
        void updateWindowTitle();
        void eyedropperSelect(entt::entity ent);
        Tool currentTool;
        bool toolLocalSpace = false;
        entt::registry& reg;
        slib::List<entt::entity> selectedEntities;
        entt::entity currentSelectedEntity;
        Transform originalObjectTransform;
        float lookX;
        float lookY;
        float cameraSpeed;
        bool imguiMetricsOpen;
        bool handleOverriden = false;
        bool entityEyedropperActive = false;
        entt::entity eyedroppedEntity = entt::null;
        uint32_t lastSaveModificationCount = 0;
        Transform* overrideTransform;
        entt::entity handleOverrideEntity = entt::null;

        void (*gameProjectSelectedCallback)(GameProject* project) = nullptr;
        void (*gameProjectClosedCallback)() = nullptr;

        EditorSettings settings;
        EngineInterfaces& interfaces;
        InputManager& inputManager;
        slib::List<std::unique_ptr<EditorWindow>> editorWindows;
        slib::List<EditorSceneView*> sceneViews;
        slib::List<AssetEditorWindow*> assetEditors;
        struct QueuedKeydown
        {
            SDL_Scancode scancode;
            ModifierFlags modifiers;
        };
        slib::List<QueuedKeydown> queuedKeydowns;
        GameState currentState = GameState::Editing;

        static void sceneLoadCallback(void* ctx, entt::registry& reg);

        friend class EditorSceneView;
    };
}
