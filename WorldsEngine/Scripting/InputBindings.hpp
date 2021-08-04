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
}
