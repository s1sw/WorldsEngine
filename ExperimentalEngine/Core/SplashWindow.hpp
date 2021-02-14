#pragma once
#include <SDL_video.h>
#include <SDL_render.h>
#include <string>

namespace worlds {
    struct SplashWindow {
        SDL_Window* win;
        SDL_Renderer* renderer;
        SDL_Surface* bgSurface;
        SDL_Texture* bgTexture;
    };

    void destroySplashWindow(SplashWindow splash);
    void redrawSplashWindow(SplashWindow splash, std::string overlay);
    SplashWindow createSplashWindow();
}
