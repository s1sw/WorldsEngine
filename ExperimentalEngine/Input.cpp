#include "Input.hpp"

InputManager::InputManager() 
	: mouseButtonFlags(0)
	, lastMouseButtonFlags(0) {

}

void InputManager::update(SDL_Window* window) {
	lastMouseButtonFlags = mouseButtonFlags;
	mouseButtonFlags = SDL_GetMouseState(nullptr, nullptr);
}

bool InputManager::mouseButtonHeld(MouseButton button) {
	uint32_t uButton = SDL_BUTTON((uint32_t)button);
	return (mouseButtonFlags & uButton) == uButton;
}

bool InputManager::mouseButtonPressed(MouseButton button) {
	uint32_t uButton = SDL_BUTTON((uint32_t)button);
	return ((mouseButtonFlags & uButton) == uButton) && ((lastMouseButtonFlags & uButton) != uButton);
}