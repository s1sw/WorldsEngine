#include "Input.hpp"
#include "../ImGui/imgui.h"
#include "Core/Log.hpp"
#include "Scripting/NetVM.hpp"
#include <SDL_gamecontroller.h>
#ifdef __linux__
#define RELATIVE_MOUSE_HACK
#endif

namespace worlds {
    enum NativeEventKind : int32_t {
        NEInvalid = -1,
        NEKeyDown,
        NEKeyUp,
        NEMouseButtonDown,
        NEMouseButtonUp,
        NEControllerButtonDown,
        NEControllerButtonUp,
        NEControllerAxisMotion
    };

    struct InputManager::NativeInputEvent {
        NativeEventKind eventKind;

        union {
            SDL_Scancode scancode;
            int mouseButtonIndex;
            SDL_GameControllerButton controllerButton;
            struct {
                SDL_GameControllerAxis controllerAxis;
                float axisValue;
            };
        };
    };

    SDL_GameController* controller;
    int controllerIndex;
    InputManager::InputManager(SDL_Window* window)
        : window(window)
        , mouseButtonFlags(0)
        , lastMouseButtonFlags(0) {
       memset(keyState, 0, sizeof(keyState));
       
       if (SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt") == -1) {
           logErr("Failed to load game controller database.");
       }

       for (int i = 0; i < SDL_NumJoysticks(); i++) {
           if (SDL_IsGameController(i)) {
               controller = SDL_GameControllerOpen(i);
               controllerIndex = i;
               logMsg("controller %i: %s", i, SDL_GameControllerName(controller));

               if (SDL_GameControllerHasRumble(controller)) {
                   SDL_GameControllerRumble(controller, 0xFFFF, 0xFFFF, 100);
               }

               break;
           }
       }
   }

    void InputManager::setScriptEngine(DotNetScriptEngine* scriptEngine) {
        this->scriptEngine = scriptEngine;
        scriptEngine->createManagedDelegate("WorldsEngine.Input.InputSystem", "ProcessNativeEvent", (void**)&processNativeEvent);
        scriptEngine->createManagedDelegate("WorldsEngine.Input.InputSystem", "EndFrame", (void**)&managedEndFrame);
    }

    void InputManager::update() {
        lastMouseButtonFlags = mouseButtonFlags;
        mouseButtonFlags = SDL_GetMouseState(nullptr, nullptr);

        SDL_GetRelativeMouseState(&mouseDelta.x, &mouseDelta.y);
        SDL_GetMouseState(&mousePos.x, &mousePos.y);

#ifdef RELATIVE_MOUSE_HACK
        int x, y;
        SDL_GetWindowSize(window, &x, &y);

        if (mouseLocked) {
            mouseDelta.x = mousePos.x - x / 2;
            mouseDelta.y = mousePos.y - y / 2;
            SDL_WarpMouseInWindow(window, x / 2, y / 2);
        }
#endif
    }

    void InputManager::processEvent(const SDL_Event& evt) {
        NativeInputEvent nativeEvent { NEInvalid };
        switch (evt.type) {
        case SDL_KEYDOWN: {
            auto scancode = evt.key.keysym.scancode;
            keyState[scancode] = true;
            nativeEvent.eventKind = NEKeyDown;
            nativeEvent.scancode = scancode;

            for (auto& handler : keydownHandlers) {
                handler(scancode);
            }
        }
            break;
        case SDL_KEYUP: {
            auto scancode = evt.key.keysym.scancode;
            keyState[scancode] = false;
            nativeEvent.eventKind = NEKeyUp;
            nativeEvent.scancode = scancode;
        }
            break;
        case SDL_MOUSEBUTTONDOWN:
            nativeEvent.eventKind = NEMouseButtonDown;
            nativeEvent.mouseButtonIndex = evt.button.button;
            break;
        case SDL_MOUSEBUTTONUP:
            nativeEvent.eventKind = NEMouseButtonUp;
            nativeEvent.mouseButtonIndex = evt.button.button;
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            nativeEvent.eventKind = NEControllerButtonDown;
            nativeEvent.controllerButton = (SDL_GameControllerButton)evt.cbutton.button;
            break;
        case SDL_CONTROLLERBUTTONUP:
            nativeEvent.eventKind = NEControllerButtonUp;
            nativeEvent.controllerButton = (SDL_GameControllerButton)evt.cbutton.button;
            break;
        case SDL_CONTROLLERAXISMOTION:
            nativeEvent.eventKind = NEControllerAxisMotion;
            nativeEvent.controllerAxis = (SDL_GameControllerAxis)evt.caxis.axis;
            nativeEvent.axisValue = (float)evt.caxis.value / 32768.0f;
            break;
        case SDL_CONTROLLERDEVICEADDED:
            controller = SDL_GameControllerOpen(evt.cdevice.which);
            controllerIndex = evt.cdevice.which;
            logMsg("controller %i: %s", evt.cdevice.which, SDL_GameControllerName(controller));

            if (SDL_GameControllerHasRumble(controller)) {
                SDL_GameControllerRumble(controller, 0xFFFF, 0xFFFF, 100);
            }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (evt.cdevice.which == controllerIndex) {
                SDL_GameControllerClose(controller);
                controller = nullptr;
            }
            break;
        }

        if (nativeEvent.eventKind != NEInvalid && processNativeEvent) {
            processNativeEvent(&nativeEvent);
        }
    }

    bool InputManager::mouseButtonHeld(MouseButton button, bool ignoreImGui) const {
        if (ImGui::GetIO().WantCaptureMouse && !ignoreImGui) return false;
        uint32_t uButton = SDL_BUTTON((uint32_t)button);
        return (mouseButtonFlags & uButton) == uButton;
    }

    bool InputManager::mouseButtonPressed(MouseButton button, bool ignoreImGui) const {
        if (ImGui::GetIO().WantCaptureMouse && !ignoreImGui) return false;
        uint32_t uButton = SDL_BUTTON((uint32_t)button);
        return ((mouseButtonFlags & uButton) == uButton) && ((lastMouseButtonFlags & uButton) != uButton);
    }

    bool InputManager::mouseButtonReleased(MouseButton button, bool ignoreImGui) const {
        if (ImGui::GetIO().WantCaptureMouse && !ignoreImGui) return false;
        uint32_t uButton = SDL_BUTTON((uint32_t)button);
        return ((mouseButtonFlags & uButton) != uButton) && ((lastMouseButtonFlags & uButton) == uButton);
    }

    bool InputManager::keyHeld(SDL_Scancode scancode, bool ignoreImGui) const {
        if (ImGui::GetIO().WantCaptureKeyboard && !ignoreImGui) return false;
        return keyState[scancode];
    }

    bool InputManager::keyPressed(SDL_Scancode scancode, bool ignoreImGui) const {
        if (ImGui::GetIO().WantCaptureKeyboard && !ignoreImGui) return false;
        return keyState[scancode] && !lastKeyState[scancode];
    }

    bool InputManager::keyReleased(SDL_Scancode scancode, bool ignoreImGui) const {
        if (ImGui::GetIO().WantCaptureKeyboard && !ignoreImGui) return false;
        return !keyState[scancode] && lastKeyState[scancode];
    }

    glm::ivec2 InputManager::getMouseDelta() const {
        return mouseDelta;
    }

    glm::ivec2 InputManager::getMousePosition() const {
        return mousePos;
    }

    void InputManager::endFrame() {
        memcpy(reinterpret_cast<void*>(lastKeyState), keyState, SDL_NUM_SCANCODES);
        if (managedEndFrame) managedEndFrame();
    }

    void InputManager::warpMouse(glm::ivec2 position) {
        SDL_WarpMouseInWindow(window, position.x, position.y);
    }

    bool InputManager::ctrlHeld() const {
        return keyHeld(SDL_SCANCODE_LCTRL, true) || keyHeld(SDL_SCANCODE_RCTRL, true);
    }

    bool InputManager::shiftHeld() const {
        return keyHeld(SDL_SCANCODE_LSHIFT, true) || keyHeld(SDL_SCANCODE_RSHIFT, true);
    }

    void InputManager::captureMouse(bool capture) {
        SDL_CaptureMouse((SDL_bool)capture);
    }

    bool InputManager::mouseLockState() const {
        return mouseLocked;
    }

    void InputManager::lockMouse(bool lock) {
#ifdef RELATIVE_MOUSE_HACK
        SDL_SetWindowGrab(window, (SDL_bool)lock);
        if (lock)
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        else
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        SDL_ShowCursor(lock ? SDL_DISABLE : SDL_ENABLE);
#else
        SDL_SetRelativeMouseMode((SDL_bool)lock);
#endif
        mouseLocked = lock;
    }

    void InputManager::triggerControllerHaptics(uint16_t leftIntensity, uint16_t rightIntensity, uint32_t duration) {
        if (controller)
            SDL_GameControllerRumble(controller, leftIntensity, rightIntensity, duration);
    }

    void InputManager::addKeydownHandler(std::function<void (SDL_Scancode)> handler) {
        keydownHandlers.add(handler);
    }
}
