#pragma once
#include <SDL2/SDL.h>
#include <glm/glm.hpp>

namespace worlds {
	enum class MouseButton : uint32_t {
		None,
		Left = SDL_BUTTON_LEFT,
		Middle = SDL_BUTTON_MIDDLE,
		Right = SDL_BUTTON_RIGHT
	};

	class InputManager {
	public:
		InputManager(SDL_Window* window);
		void update();
		void endFrame();
		bool mouseButtonHeld(MouseButton button, bool ignoreImGui = false);
		bool mouseButtonPressed(MouseButton button, bool ignoreImGui = false);
		bool mouseButtonReleased(MouseButton button, bool ignoreImGui = false);
		bool keyHeld(SDL_Scancode scancode, bool ignoreImGui = false);
		bool keyPressed(SDL_Scancode scancode, bool ignoreImGui = false);
		bool keyReleased(SDL_Scancode scancode, bool ignoreImGui = false);
		glm::ivec2 getMouseDelta();
		glm::ivec2 getMousePosition();
		void warpMouse(glm::ivec2 newPosition);
	private:
		SDL_Window* window;
		uint32_t mouseButtonFlags;
		uint32_t lastMouseButtonFlags;
		const Uint8* keyState;
		Uint8 lastKeyState[SDL_NUM_SCANCODES];
		glm::ivec2 mouseDelta;
		glm::ivec2 mousePos;
	};
}