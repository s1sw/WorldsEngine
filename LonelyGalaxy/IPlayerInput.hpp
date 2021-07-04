#pragma once
#include <glm/vec2.hpp>

namespace lg {
    class IPlayerInput {
    public:
        virtual bool sprint() = 0;
        // Movement on the XZ axes in world space
        virtual glm::vec2 movementInput() = 0;
        virtual bool consumeJump() = 0;
    };
}
