#pragma once
#include <string>
#include <thread>

#include <SDL_video.h>
#include <SDL_render.h>
#include <SDL_surface.h>

namespace worlds {
    class SplashWindow {
    public:
        SplashWindow(bool small);
        void changeOverlay(const std::string& overlay);
        ~SplashWindow();
    private:
        void redraw();
        void eventThread();

        bool small;

        volatile bool windowCreated = false;
        bool running = true;

        SDL_Window* win = nullptr;
        SDL_Renderer* renderer;
        SDL_Surface* bgSurface;
        SDL_Texture* bgTexture;

        SDL_Surface* overlaySurface;
        SDL_Texture* overlayTexture;

        std::thread winThread;
        std::string overlay;
        std::string loadedOverlay;
    };
}
