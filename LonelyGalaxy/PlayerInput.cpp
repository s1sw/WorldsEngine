#include "PlayerInput.hpp"
#include "Render/Camera.hpp"
#include <glm/gtx/norm.hpp>
#include <Input/Input.hpp>
#include <VR/IVRInterface.hpp>

namespace lg {
    KeyboardPlayerInput::KeyboardPlayerInput(worlds::EngineInterfaces interfaces)
        : interfaces(interfaces) {
    }

    void KeyboardPlayerInput::update() {
        jumpQueued = interfaces.inputManager->keyPressed(SDL_SCANCODE_SPACE);
    }

    bool KeyboardPlayerInput::sprint() {
        return interfaces.inputManager->keyHeld(SDL_SCANCODE_LSHIFT);
    }

    glm::vec2 KeyboardPlayerInput::movementInput() {
        glm::vec2 inputDir { 0.0f };

        if (interfaces.inputManager->keyHeld(SDL_SCANCODE_W)) {
            inputDir.y += 1.0f;
        }

        if (interfaces.inputManager->keyHeld(SDL_SCANCODE_S)) {
            inputDir.y -= 1.0f;
        }

        if (interfaces.inputManager->keyHeld(SDL_SCANCODE_A)) {
            inputDir.x += 1.0f;
        }

        if (interfaces.inputManager->keyHeld(SDL_SCANCODE_D)) {
            inputDir.x -= 1.0f;
        }

        if (glm::length2(inputDir) > 0.0f) {
            inputDir = glm::normalize(inputDir);
        }

        glm::vec3 inputDirCS = glm::vec3{inputDir.x, 0.0f, inputDir.y};
        inputDirCS = inputDirCS * glm::inverse(interfaces.mainCamera->rotation);
        inputDirCS.y = 0.0f;

        inputDir = glm::vec2{inputDirCS.x, inputDirCS.z};

        if (glm::length2(inputDir) > 0.0f) {
            inputDir = glm::normalize(inputDir);
        }

        return inputDir;
    }

    bool KeyboardPlayerInput::consumeJump() {
        if (jumpQueued) {
            jumpQueued = false;
            return true;
        }

        return false;
    }

    VRPlayerInput::VRPlayerInput(worlds::EngineInterfaces interfaces)
        : interfaces(interfaces) {
    }

    void VRPlayerInput::update() {
        jumpQueued = interfaces.vrInterface->getJumpInput();
    }

    bool VRPlayerInput::sprint() {
        return interfaces.vrInterface->getSprintInput();
    }

    glm::vec2 VRPlayerInput::movementInput() {
        glm::vec2 locIn = interfaces.vrInterface->getLocomotionInput();
        locIn = glm::vec2 { locIn.x, -locIn.y };

        float maxLength = glm::max(glm::abs(locIn.x), glm::abs(locIn.y));

        glm::vec3 desiredVel{ locIn.x, 0.0f, locIn.y };

        glm::mat4 camMat = interfaces.vrInterface->getHeadTransform();

        desiredVel = camMat * glm::vec4 { desiredVel, 0.0f };
        desiredVel = -desiredVel * glm::inverse(interfaces.mainCamera->rotation);

        if (glm::length2(desiredVel) > 0.0f) {
            desiredVel = glm::normalize(desiredVel) * maxLength;
        }

        return glm::vec2 { desiredVel.x, desiredVel.z };
    }

    bool VRPlayerInput::consumeJump() {
        if (jumpQueued) {
            jumpQueued = false;
            return true;
        }

        return false;
    }
}
