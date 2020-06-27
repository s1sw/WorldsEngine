#pragma once
#include <SDL2/SDL.h>

enum class MouseButton : uint32_t {
	None,
	Left = SDL_BUTTON_LEFT,
	Middle = SDL_BUTTON_MIDDLE,
	Right = SDL_BUTTON_RIGHT
};

class InputManager {
public:
	InputManager();
	void update(SDL_Window* window);
	bool mouseButtonHeld(MouseButton button);
	bool mouseButtonPressed(MouseButton button);
private:
	uint32_t mouseButtonFlags;
	uint32_t lastMouseButtonFlags;
};