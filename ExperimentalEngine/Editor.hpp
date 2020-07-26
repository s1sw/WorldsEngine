#pragma once
#include <entt/entt.hpp>
#include "Input.hpp"
#include "Camera.hpp"
#include "Transform.hpp"

enum class Tool {
    None,
    Translate,
    Rotate,
    Scale
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

class Editor {
public:
    Editor(entt::registry& reg, InputManager& inputManager, Camera& cam);
    void select(entt::entity entity);
    void update(float deltaTime);
private:
    void updateCamera(float deltaTime);
    void handleAxisButtonPress(AxisFlagBits axis);
    Tool currentTool;
    AxisFlagBits currentAxisLock;
    entt::registry& reg;
    entt::entity currentSelectedEntity;
    InputManager& inputManager;
    Camera& cam;
    Transform originalObjectTransform;
    float startingMouseDistance;
    float lookX;
    float lookY;

    bool objectSnapGlobal;
};