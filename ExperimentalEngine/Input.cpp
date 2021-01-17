#include "PCH.hpp"
#include "Input.hpp"
#include "imgui.h"

namespace worlds {
	InputManager::InputManager(SDL_Window* window)
		:  window(window) 
        , mouseButtonFlags(0)
		, lastMouseButtonFlags(0) {
		keyState = SDL_GetKeyboardState(nullptr);
	}

	void InputManager::update() {
		lastMouseButtonFlags = mouseButtonFlags;
		mouseButtonFlags = SDL_GetMouseState(nullptr, nullptr);

		//keyState = SDL_GetKeyboardState(nullptr);
		SDL_GetRelativeMouseState(&mouseDelta.x, &mouseDelta.y);
		SDL_GetMouseState(&mousePos.x, &mousePos.y);
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
		std::memcpy(reinterpret_cast<void*>(lastKeyState), keyState, SDL_NUM_SCANCODES);
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

	void InputManager::lockMouse(bool lock) {
		SDL_SetRelativeMouseMode((SDL_bool)lock);
	}
}
