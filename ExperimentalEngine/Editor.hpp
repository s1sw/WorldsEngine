#pragma once
#include <entt/entt.hpp>
#include "Input.hpp"
#include "Camera.hpp"
#include "Transform.hpp"
#include "IGameEventHandler.hpp"

namespace worlds {
    typedef uint32_t AssetID;

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
        virtual void setActive(bool active) { this->active = active; }
        virtual void draw(entt::registry& reg) = 0;
        virtual const char* getName() = 0;
    protected:
        EngineInterfaces interfaces;
        Editor* editor;
        bool active;
    };

    class Editor {
    public:
        Editor(entt::registry& reg, EngineInterfaces interfaces);
        void select(entt::entity entity);
        void update(float deltaTime);
        void activateTool(Tool newTool);
        void setActive(bool active) { this->active = active; }
        entt::entity getSelectedEntity() { return currentSelectedEntity; }
    private:
        void updateCamera(float deltaTime);
        void handleAxisButtonPress(AxisFlagBits axis);
        Tool currentTool;
        Tool lastActiveTool;
        AxisFlagBits currentAxisLock;
        entt::registry& reg;
        entt::entity currentSelectedEntity;
        Camera& cam;
        Transform originalObjectTransform;
        float startingMouseDistance;
        float lookX;
        float lookY;
        bool imguiMetricsOpen;
        bool enableTransformGadget;
        bool active;

        EditorSettings settings;
        EngineInterfaces interfaces;
        InputManager& inputManager;
        RTTPassHandle sceneViewPass;
        std::vector<std::unique_ptr<EditorWindow>> editorWindows;
        vk::DescriptorSet sceneViewDS;
    };
}