#pragma once
#include <entt/entt.hpp>
#include "../Input/Input.hpp"
#include "../Render/Camera.hpp"
#include "../Core/Transform.hpp"
#include "../Core/IGameEventHandler.hpp"
#include "UITextureManager.hpp"
#include <deque>
#include <slib/List.hpp>
#include <string>

struct VkDescriptorSet_T;
typedef VkDescriptorSet_T* VkDescriptorSet;

namespace worlds {
    typedef uint32_t AssetID;
    class RTTPass;

    enum class Tool {
        None = 0,
        Translate = 1,
        Rotate = 2,
        Scale = 4,
        Bounds = 8
    };

    enum class AxisFlagBits {
        None = 0,
        X = 1,
        Y = 2,
        Z = 4,
        All = 7
    };

    inline AxisFlagBits operator |(AxisFlagBits lhs, AxisFlagBits rhs) {
        return static_cast<AxisFlagBits> (
            static_cast<unsigned>(lhs) |
            static_cast<unsigned>(rhs)
        );
    }

    inline AxisFlagBits operator &(AxisFlagBits lhs, AxisFlagBits rhs) {
        return static_cast<AxisFlagBits> (
            static_cast<unsigned>(lhs) &
            static_cast<unsigned>(rhs)
        );
    }

    inline AxisFlagBits operator ^(AxisFlagBits lhs, AxisFlagBits rhs) {
        return static_cast<AxisFlagBits> (
            static_cast<unsigned>(lhs) ^
            static_cast<unsigned>(rhs)
        );
    }

    inline Tool operator |(Tool lhs, Tool rhs) {
        return static_cast<Tool>(
            static_cast<unsigned>(lhs) |
            static_cast<unsigned>(rhs)
        );
    }

    inline Tool operator &(Tool lhs, Tool rhs) {
        return static_cast<Tool> (
            static_cast<unsigned>(lhs) &
            static_cast<unsigned>(rhs)
        );
    }

    struct EditorSettings {
        bool objectSnapGlobal = false;
        float snapIncrement = 0.1f;
        float angularSnapIncrement = 15.0f;
        bool enableShadows = true;
    };

    class Editor;

    enum class EditorMenu {
        File,
        Edit,
        Window,
        Help
    };
    
    class GameProject {
    public:
        GameProject(std::string path);
        std::string_view name() const;
        std::string_view root() const;
        void mountPaths();
        void unmountPaths();
    private:
        std::string _name;
        std::string _root;
        std::string _srcDataPath;
        std::string _compiledDataPath;
        std::string _rawPath;
        std::vector<std::string> _copyDirs;
    };

    class EditorWindow {
    public:
        EditorWindow(EngineInterfaces interfaces, Editor* editor)
            : interfaces(interfaces)
            , editor(editor)
            , active(true) {}

        virtual bool isActive() { return active; }
        virtual void setActive(bool active) { this->active = active; }
        virtual void draw(entt::registry& reg) = 0;
        virtual EditorMenu menuSection() { return EditorMenu::Window; }
        virtual const char* getName() = 0;
        virtual ~EditorWindow() {};
    protected:
        EngineInterfaces interfaces;
        Editor* editor;
        bool active;
    };

    class EditorUndo {
    public:
        void pushState(entt::registry& reg);
        void undo(entt::registry& reg);
        void redo(entt::registry& reg);
        void clear();
        uint32_t modificationCount() { return currentPos; }
    private:
        uint32_t highestSaved = 0;
        uint32_t currentPos = 0;
    };

    class EditorSceneView {
    public:
        EditorSceneView(EngineInterfaces interfaces, Editor* ed);
        void drawWindow(int unqiueId);
        void recreateRTT();
        void setShadowsEnabled(bool enabled);
        void setViewportActive(bool active);
        bool open = true;
        ~EditorSceneView();
    private:
        void updateCamera(float deltaTime);
        uint32_t currentWidth, currentHeight;
        VkDescriptorSet sceneViewDS = nullptr;
        RTTPass* sceneViewPass = nullptr;
        Camera cam;
        EngineInterfaces interfaces;
        Editor* ed;
        float lookX = 0.0f;
        float lookY = 0.0f;
        bool shadowsEnabled = true;
        bool viewportActive = true;
    };

    struct EntityFolder {
        std::string name;
        std::vector<EntityFolder> children;
        std::vector<entt::entity> entities;
    };

    struct EntityFolders {
        EntityFolder rootFolder;
    };

    class Editor {
    public:
        Editor(entt::registry& reg, EngineInterfaces interfaces);
        ~Editor();
        void select(entt::entity entity);
        void multiSelect(entt::entity entity);
        void update(float deltaTime);
        void activateTool(Tool newTool);
        entt::entity getSelectedEntity() { return currentSelectedEntity; }
        const slib::List<entt::entity>& getSelectedEntities() const { return selectedEntities; }
        bool isEntitySelected(entt::entity ent) const;
        UITextureManager* texManager() { return texMan; }
        EditorUndo undo;
        bool active = true;
        void overrideHandle(Transform* t);
        bool entityEyedropper(entt::entity& picked);
        AssetID currentSelectedAsset;
        const GameProject& currentProject() { return *project; }
    private:
        std::unique_ptr<GameProject> project;
        ImTextureID titleBarIcon;
        void drawMenuBarTitle();
        void handleTools(Transform& t, ImVec2 wPos, ImVec2 wSize, Camera& camera);
        std::string generateWindowTitle();
        void updateWindowTitle();
        void openProject(std::string projectPath);
        void eyedropperSelect(entt::entity ent);
        Tool currentTool;
        bool toolLocalSpace = false;
        entt::registry& reg;
        slib::List<entt::entity> selectedEntities;
        entt::entity currentSelectedEntity;
        Camera& cam;
        Transform originalObjectTransform;
        float lookX;
        float lookY;
        float cameraSpeed;
        bool imguiMetricsOpen;
        bool handleOverriden = false;
        bool entityEyedropperActive = false;
        entt::entity eyedroppedEntity = entt::null;
        int lastSaveModificationCount = 0;
        Transform* overrideTransform;

        UITextureManager* texMan;

        EditorSettings settings;
        EngineInterfaces interfaces;
        InputManager& inputManager;
        slib::List<std::unique_ptr<EditorWindow>> editorWindows;
        slib::List<EditorSceneView*> sceneViews;

        friend class EditorSceneView;
    };
}

