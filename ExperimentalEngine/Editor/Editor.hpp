#pragma once
#include <entt/entt.hpp>
#include "../Input/Input.hpp"
#include "../Render/Camera.hpp"
#include "../Core/Transform.hpp"
#include "../Core/IGameEventHandler.hpp"
#include "UITextureManager.hpp"
#include <deque>

struct VkDescriptorSet_T;
typedef VkDescriptorSet_T* VkDescriptorSet;

namespace worlds {
    typedef uint32_t AssetID;
    typedef uint32_t RTTPassHandle;

    enum class Tool {
        None,
        Translate,
        Rotate,
        Scale,
        Bounds
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

    struct EditorSettings {
        EditorSettings()
            : objectSnapGlobal(false)
            , scaleSnapIncrement(0.1f) {
        }
        bool objectSnapGlobal;
        float scaleSnapIncrement;
    };

    class Editor;

    class EditorWindow {
    public:
        EditorWindow(EngineInterfaces interfaces, Editor* editor) 
            : interfaces(interfaces)
            , editor(editor)
            , active(true) {}

        virtual bool isActive() { return active; }
        virtual bool showInWindowList() { return true; }
        virtual void setActive(bool active) { this->active = active; }
        virtual void draw(entt::registry& reg) = 0;
        virtual const char* getName() = 0;
        virtual ~EditorWindow() {};
    protected:
        EngineInterfaces interfaces;
        Editor* editor;
        bool active;
    };

    class EditorUndo {
    public:
        void pushState();
        void undo();
        void redo();
        void setMaxStackSize(uint32_t max);
    private:
        void removeEnd();

        uint32_t maxStackSize = 64;
        uint32_t currentPos = 0; 
        std::deque<std::string> undoStack;
    };

    class Editor {
    public:
        Editor(entt::registry& reg, EngineInterfaces interfaces);
        void select(entt::entity entity);
        void update(float deltaTime);
        void activateTool(Tool newTool);
        void setActive(bool active);
        entt::entity getSelectedEntity() { return currentSelectedEntity; }
        UITextureManager* texManager() { return texMan; }
    private:
        void updateCamera(float deltaTime);
        std::string generateWindowTitle();
        void updateWindowTitle();
        Tool currentTool;
        bool toolLocalSpace = false;
        entt::registry& reg;
        entt::entity currentSelectedEntity;
        Camera& cam;
        Transform originalObjectTransform;
        float lookX;
        float lookY;
        float cameraSpeed;
        bool imguiMetricsOpen;
        bool active;

        UITextureManager* texMan;

        EditorSettings settings;
        EngineInterfaces interfaces;
        InputManager& inputManager;
        RTTPassHandle sceneViewPass;
        std::vector<std::unique_ptr<EditorWindow>> editorWindows;
        VkDescriptorSet sceneViewDS;
    };
}

