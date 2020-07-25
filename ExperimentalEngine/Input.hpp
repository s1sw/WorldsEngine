#pragma once
#include <SDL2/SDL.h>
#include <glm/glm.hpp>

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
	void endFrame();
	bool mouseButtonHeld(MouseButton button);
	bool mouseButtonPressed(MouseButton button);
	bool mouseButtonReleased(MouseButton button);
	bool keyHeld(SDL_Scancode scancode);
	bool keyPressed(SDL_Scancode scancode);
	bool keyReleased(SDL_Scancode scancode);
	glm::ivec2 getMouseDelta() { return mouseDelta; }
	glm::ivec2 getMousePosition() { return mousePos; }
private:
	uint32_t mouseButtonFlags;
	uint32_t lastMouseButtonFlags;
	const Uint8* keyState;
	Uint8 lastKeyState[SDL_NUM_SCANCODES];
	glm::ivec2 mouseDelta;
	glm::ivec2 mousePos;
};