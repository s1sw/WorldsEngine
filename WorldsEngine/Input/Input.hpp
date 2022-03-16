#pragma once
#include <slib/List.hpp>
#include <SDL.h>
#include <functional>
#include <glm/glm.hpp>

namespace worlds {
    class DotNetScriptEngine;
    enum class MouseButton : uint32_t {
        None,
        Left = SDL_BUTTON_LEFT,
        Middle = SDL_BUTTON_MIDDLE,
        Right = SDL_BUTTON_RIGHT
    };

    class InputManager {
        public:
            InputManager(SDL_Window* window);
            void setScriptEngine(DotNetScriptEngine*);
            void update();
            void processEvent(const SDL_Event& evt);
            void endFrame();
            bool mouseButtonHeld(MouseButton button, bool ignoreImGui = false) const;
            bool mouseButtonPressed(MouseButton button, bool ignoreImGui = false) const;
            bool mouseButtonReleased(MouseButton button, bool ignoreImGui = false) const;
            bool keyHeld(SDL_Scancode scancode, bool ignoreImGui = false) const;
            bool keyPressed(SDL_Scancode scancode, bool ignoreImGui = false) const;
            bool keyReleased(SDL_Scancode scancode, bool ignoreImGui = false) const;
            glm::ivec2 getMouseDelta() const;
            glm::ivec2 getMousePosition() const;
            void warpMouse(glm::ivec2 newPosition);
            bool ctrlHeld() const;
            bool shiftHeld() const;
            void captureMouse(bool capture);
            bool mouseLockState() const;
            void lockMouse(bool lock);
            void triggerControllerHaptics(uint16_t leftIntensity, uint16_t rightIntensity, uint32_t duration);
            void addKeydownHandler(std::function<void(SDL_Scancode)> handler);
        private:
            slib::List<std::function<void(SDL_Scancode)>> keydownHandlers;
            SDL_Window* window;
            uint32_t mouseButtonFlags;
            uint32_t lastMouseButtonFlags;
            Uint8 keyState[SDL_NUM_SCANCODES];
            Uint8 lastKeyState[SDL_NUM_SCANCODES];
            glm::ivec2 mouseDelta;
            glm::ivec2 mousePos;
            bool mouseLocked = false;
            DotNetScriptEngine* scriptEngine = nullptr;
            struct NativeInputEvent;
            void(*processNativeEvent)(NativeInputEvent*) = nullptr;
            void(*managedEndFrame)() = nullptr;
    };
}
