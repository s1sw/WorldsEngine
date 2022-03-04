#include "Export.hpp"
#include "Input/Input.hpp"

using namespace worlds;

InputManager* csharpInputManager;

extern "C" {
    EXPORT int input_getKeyHeld(int keyCode) {
        return csharpInputManager->keyHeld((SDL_Scancode)keyCode);
    }

    EXPORT int input_getKeyPressed(int keyCode) {
        return csharpInputManager->keyPressed((SDL_Scancode)keyCode);
    }

    EXPORT int input_getKeyReleased(int keyCode) {
        return csharpInputManager->keyReleased((SDL_Scancode)keyCode);
    }

    EXPORT void input_getMousePosition(glm::vec2* posOut) {
        *posOut = csharpInputManager->getMousePosition();
    }

    EXPORT void input_setMousePosition(glm::vec2* pos) {
        csharpInputManager->warpMouse(*pos);
    }

    EXPORT void input_getMouseDelta(glm::vec2* deltaOut) {
        *deltaOut = csharpInputManager->getMouseDelta();
    }

    EXPORT int input_getMouseButtonPressed(MouseButton button) {
        return csharpInputManager->mouseButtonPressed(button);
    }

    EXPORT int input_getMouseButtonReleased(MouseButton button) {
        return csharpInputManager->mouseButtonReleased(button);
    }
    
    EXPORT int input_getMouseButtonHeld(MouseButton button) {
        return csharpInputManager->mouseButtonHeld(button);
    }

    EXPORT void input_triggerControllerHaptics(uint16_t leftIntensity, uint16_t rightIntensity, uint32_t duration) {
        return csharpInputManager->triggerControllerHaptics(leftIntensity, rightIntensity, duration);
    }
}
