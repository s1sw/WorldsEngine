#pragma once
#include <SDL_events.h>
#include <SDL_video.h>
#include <functional>
#include <vector>

namespace worlds
{
    class InputManager;

    class Window
    {
      public:
        Window(const char* title, int w, int h, bool startHidden = false);
        Window(Window& other) = delete;
        ~Window();

        void bindInputManager(InputManager* manager);
        void processEvents();

        void addFileDropHandler(std::function<void(const char*)> handler);
        void show();
        void hide();
        void raise();
        void maximise();
        void minimise();
        void restore();
        void resize(int width, int height);
        void setTitle(const char* title);
        void setFullscreen(bool fullscreen);

        const char* getTitle();
        bool shouldQuit();
        bool isMaximised();
        bool isMinimised();
        bool isFocused();
        bool isFullscreen();
        void getSize(int* width, int* height);
        SDL_Window* getWrappedHandle();

      private:
        std::vector<std::function<void(const char*)>> fileDropHandlers;
        void processEvent(SDL_Event& evt);
        bool hasFlag(uint32_t flag);
        InputManager* inputManager;
        SDL_Window* window;
        bool _shouldQuit;
    };
}
