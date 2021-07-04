#pragma once
#include "IPlayerInput.hpp"
#include <Core/IGameEventHandler.hpp>

namespace lg {
    class KeyboardPlayerInput : public IPlayerInput {
    public:
        KeyboardPlayerInput(worlds::EngineInterfaces interfaces);
        bool sprint() override;
        glm::vec2 movementInput() override;
        bool consumeJump() override;
    };
}
