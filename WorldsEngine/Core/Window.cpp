#include "Window.hpp"
#include <Core/Log.hpp>
#include <Input/Input.hpp>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_sdl.h>
#include <SDL_hints.h>
#include <SDL_syswm.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace worlds {
    Window::Window(const char* title, int w, int h, bool startHidden)
        : inputManager(nullptr)
        , _shouldQuit(false) {
        uint32_t flags =
            SDL_WINDOW_VULKAN
            | SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_ALLOW_HIGHDPI;

        if (startHidden) flags |= SDL_WINDOW_HIDDEN;
        SDL_SetHint("SDL_BORDERLESS_RESIZABLE_STYLE", "1");
        SDL_SetHint("SDL_BORDERLESS_WINDOWED_STYLE", "1");

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

        if (evt.type == SDL_WINDOWEVENT_SIZE_CHANGED) {
            logMsg("resized to %ix%i", evt.window.data1, evt.window.data2);
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

#ifdef _WIN32
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        SDL_SysWMinfo wminfo;
        SDL_VERSION(&wminfo.version);
        SDL_GetWindowWMInfo(window, &wminfo);
        HWND hwnd = wminfo.info.win.window;
        HRGN rgn = CreateRectRgn(8, 8, w - 8, h - 8);
        SetWindowRgn(hwnd, rgn, true);
#endif
    }

    void Window::minimise() {
        SDL_MinimizeWindow(window);
    }

    void Window::restore() {
        SDL_RestoreWindow(window);
#ifdef _WIN32
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        SDL_SysWMinfo wminfo;
        SDL_VERSION(&wminfo.version);
        SDL_GetWindowWMInfo(window, &wminfo);
        HWND hwnd = wminfo.info.win.window;
        SetWindowRgn(hwnd, nullptr, true);
#endif
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
        if (isMaximised()) {
            //width -= 8;
            //height -= 8;
        }
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
