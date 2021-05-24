#pragma once
#include <entt/entt.hpp>
#include "../Input/Input.hpp"
#include "../Render/Camera.hpp"
#include "../Core/Transform.hpp"
#include "../Core/IGameEventHandler.hpp"
#include "UITextureManager.hpp"
#include <deque>
#include <slib/List.hpp>

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
    };

    class Editor;

    enum class EditorMenu {
        File,
        Edit,
        Window,
        Help
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
        void clear() { highestSaved = 0; currentPos = 0; }
    private:
        uint32_t highestSaved = 0;
        uint32_t currentPos = 0;
    };

    class Editor {
    public:
        Editor(entt::registry& reg, EngineInterfaces interfaces);
        void select(entt::entity entity);
        void multiSelect(entt::entity entity);
        void update(float deltaTime);
        void activateTool(Tool newTool);
        entt::entity getSelectedEntity() { return currentSelectedEntity; }
        UITextureManager* texManager() { return texMan; }
        EditorUndo undo;
        bool active = true;
        void overrideHandle(Transform* t);
    private:
        void sceneWindow();
        void handleTools(Transform& t, ImVec2 wPos, ImVec2 wSize);
        void updateCamera(float deltaTime);
        std::string generateWindowTitle();
        void updateWindowTitle();
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
        Transform* overrideTransform;

        UITextureManager* texMan;

        EditorSettings settings;
        EngineInterfaces interfaces;
        InputManager& inputManager;
        RTTPass* sceneViewPass;
        std::vector<std::unique_ptr<EditorWindow>> editorWindows;
        VkDescriptorSet sceneViewDS;
    };
}

