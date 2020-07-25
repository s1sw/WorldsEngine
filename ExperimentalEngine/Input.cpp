#include "PCH.hpp"
#include "Input.hpp"

InputManager::InputManager() 
	: mouseButtonFlags(0)
	, lastMouseButtonFlags(0) {
	keyState = SDL_GetKeyboardState(nullptr);
}

void InputManager::update(SDL_Window* window) {
	lastMouseButtonFlags = mouseButtonFlags;
	mouseButtonFlags = SDL_GetMouseState(nullptr, nullptr);

	//keyState = SDL_GetKeyboardState(nullptr);
	SDL_GetRelativeMouseState(&mouseDelta.x, &mouseDelta.y);
	SDL_GetMouseState(&mousePos.x, &mousePos.y);
}

bool InputManager::mouseButtonHeld(MouseButton button) {
	uint32_t uButton = SDL_BUTTON((uint32_t)button);
	return (mouseButtonFlags & uButton) == uButton;
}

bool InputManager::mouseButtonPressed(MouseButton button) {
	uint32_t uButton = SDL_BUTTON((uint32_t)button);
	return ((mouseButtonFlags & uButton) == uButton) && ((lastMouseButtonFlags & uButton) != uButton);
}

bool InputManager::mouseButtonReleased(MouseButton button) {
	uint32_t uButton = SDL_BUTTON((uint32_t)button);
	return ((mouseButtonFlags & uButton) != uButton) && ((lastMouseButtonFlags & uButton) == uButton);
}

bool InputManager::keyHeld(SDL_Scancode scancode) {
	return keyState[scancode];
}

bool InputManager::keyPressed(SDL_Scancode scancode) {
	return keyState[scancode] && !lastKeyState[scancode];
}

bool InputManager::keyReleased(SDL_Scancode scancode) {
	return !keyState[scancode] && lastKeyState[scancode];
}

void InputManager::endFrame() {
	std::memcpy(reinterpret_cast<void*>(lastKeyState), keyState, SDL_NUM_SCANCODES);
}