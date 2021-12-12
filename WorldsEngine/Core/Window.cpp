#include "Window.hpp"
#include <Core/Log.hpp>
#include <Input/Input.hpp>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_sdl.h>

namespace worlds {
    Window::Window(const char* title, int w, int h, bool startHidden)
        : _shouldQuit(false)
        , inputManager(nullptr) {
        uint32_t flags =
            SDL_WINDOW_VULKAN
            | SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_ALLOW_HIGHDPI;

        if (startHidden) flags |= SDL_WINDOW_HIDDEN;

        window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            w, h, flags);
    }

    void Window::bindInputManager(InputManager* manager) {
        inputManager = manager;
    }

    void Window::processEvents() {
        SDL_Event evt;
        while (SDL_PollEvent(&evt)) {
            processEvent(evt);
        }
    }

    void Window::processEvent(SDL_Event& evt) {
        if (evt.type == SDL_QUIT) {
            _shouldQuit = true;
            return;
        }

        if (evt.type == SDL_DROPFILE) {
            logMsg("file dropped: %s", evt.drop.file);

            for (auto& handler : fileDropHandlers) {
                handler(evt.drop.file);
            }

            SDL_free(evt.drop.file);
            return;
        }

        if (inputManager)
            inputManager->processEvent(evt);

        if (ImGui::GetCurrentContext())
            ImGui_ImplSDL2_ProcessEvent(&evt);
    }

    void Window::addFileDropHandler(std::function<void(const char*)> handler) {
        fileDropHandlers.push_back(handler);
    }

    void Window::show() {
        SDL_ShowWindow(window);
    }

    void Window::hide() {
        SDL_HideWindow(window);
    }

    void Window::raise() {
        SDL_RaiseWindow(window);
    }

    void Window::maximise() {
        SDL_MaximizeWindow(window);
    }

    void Window::minimise() {
        SDL_MinimizeWindow(window);
    }

    void Window::restore() {
        SDL_RestoreWindow(window);
    }

    void Window::resize(int width, int height) {
        SDL_SetWindowSize(window, width, height);
    }

    void Window::setTitle(const char* title) {
        SDL_SetWindowTitle(window, title);
    }

    void Window::setFullscreen(bool fullscreen) {
        SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }

    const char* Window::getTitle() {
        return SDL_GetWindowTitle(window);
    }

    bool Window::shouldQuit() {
        return _shouldQuit;
    }

    bool Window::isMaximised() {
        return hasFlag(SDL_WINDOW_MAXIMIZED);
    }

    bool Window::isMinimised() {
        return hasFlag(SDL_WINDOW_MINIMIZED);
    }

    bool Window::isFocused() {
        return hasFlag(SDL_WINDOW_INPUT_FOCUS);
    }

    void Window::getSize(int* width, int* height) {
        SDL_GetWindowSize(window, width, height);
    }

    SDL_Window* Window::getWrappedHandle() {
        return window;
    }

    bool Window::hasFlag(uint32_t flag) {
        return (SDL_GetWindowFlags(window) & flag) == flag;
    }

    Window::~Window() {
        SDL_DestroyWindow(window);
    }
}