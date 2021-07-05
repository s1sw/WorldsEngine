#pragma once
#include "IPlayerInput.hpp"
#include <Core/IGameEventHandler.hpp>

namespace lg {
    class KeyboardPlayerInput : public IPlayerInput {
    public:
        KeyboardPlayerInput(worlds::EngineInterfaces interfaces);
        void update() override;

        bool sprint() override;
        glm::vec2 movementInput() override;
        bool consumeJump() override;
    private:
        worlds::EngineInterfaces interfaces;
        bool jumpQueued = false;
    };

    class VRPlayerInput : public IPlayerInput {
    public:
        VRPlayerInput(worlds::EngineInterfaces interfaces);
        void update() override;

        bool sprint() override;
        glm::vec2 movementInput() override;
        bool consumeJump() override;
    private:
        worlds::EngineInterfaces interfaces;
    };
}
