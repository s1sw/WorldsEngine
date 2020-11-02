#include "PCH.hpp"
#include "Input.hpp"
#include "imgui.h"

InputManager::InputManager(SDL_Window* window) 
	: mouseButtonFlags(0)
	, lastMouseButtonFlags(0)
	, window(window) {
	keyState = SDL_GetKeyboardState(nullptr);
}

void InputManager::update() {
	lastMouseButtonFlags = mouseButtonFlags;
	mouseButtonFlags = SDL_GetMouseState(nullptr, nullptr);

	//keyState = SDL_GetKeyboardState(nullptr);
	SDL_GetRelativeMouseState(&mouseDelta.x, &mouseDelta.y);
	SDL_GetMouseState(&mousePos.x, &mousePos.y);
}

bool InputManager::mouseButtonHeld(MouseButton button, bool ignoreImGui) {
	if (ImGui::GetIO().WantCaptureMouse && !ignoreImGui) return false;
	uint32_t uButton = SDL_BUTTON((uint32_t)button);
	return (mouseButtonFlags & uButton) == uButton;
}

bool InputManager::mouseButtonPressed(MouseButton button, bool ignoreImGui) {
	if (ImGui::GetIO().WantCaptureMouse && !ignoreImGui) return false;
	uint32_t uButton = SDL_BUTTON((uint32_t)button);
	return ((mouseButtonFlags & uButton) == uButton) && ((lastMouseButtonFlags & uButton) != uButton);
}

bool InputManager::mouseButtonReleased(MouseButton button, bool ignoreImGui) {
	if (ImGui::GetIO().WantCaptureMouse && !ignoreImGui) return false;
	uint32_t uButton = SDL_BUTTON((uint32_t)button);
	return ((mouseButtonFlags & uButton) != uButton) && ((lastMouseButtonFlags & uButton) == uButton);
}

bool InputManager::keyHeld(SDL_Scancode scancode) {
	if (ImGui::GetIO().WantCaptureKeyboard) return false;
	return keyState[scancode];
}

bool InputManager::keyPressed(SDL_Scancode scancode) {
	if (ImGui::GetIO().WantCaptureKeyboard) return false;
	return keyState[scancode] && !lastKeyState[scancode];
}

bool InputManager::keyReleased(SDL_Scancode scancode) {
	if (ImGui::GetIO().WantCaptureKeyboard) return false;
	return !keyState[scancode] && lastKeyState[scancode];
}

glm::ivec2 InputManager::getMouseDelta() {
	return mouseDelta;
}

glm::ivec2 InputManager::getMousePosition() {
	return mousePos;
}

void InputManager::endFrame() {
	std::memcpy(reinterpret_cast<void*>(lastKeyState), keyState, SDL_NUM_SCANCODES);
}

void InputManager::warpMouse(glm::ivec2 position) {
	SDL_WarpMouseInWindow(window, position.x, position.y);
}