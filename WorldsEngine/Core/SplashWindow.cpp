#include "SplashWindow.hpp"
#include <SDL_events.h>
#include "Fatal.hpp"
#include "EarlySDLUtil.hpp"
#include "Log.hpp"

namespace worlds {
    void destroySplashWindow(SplashWindow splash) {
        SDL_DestroyTexture(splash.bgTexture);
        SDL_DestroyRenderer(splash.renderer);
        SDL_FreeSurface(splash.bgSurface);
        SDL_DestroyWindow(splash.win);
    }

    void redrawSplashWindow(SplashWindow splash, std::string overlay) {
        SDL_PumpEvents();

        SDL_RenderClear(splash.renderer);
        SDL_RenderCopy(splash.renderer, splash.bgTexture, nullptr, nullptr);

        if (!overlay.empty()) {
            SDL_Surface* s = loadDataFileToSurface("SplashText/" + overlay + ".png");
            SDL_Texture* t = SDL_CreateTextureFromSurface(splash.renderer, s);

            SDL_Rect targetRect;
            targetRect.x = 544;
            targetRect.y = 546;
            targetRect.w = 256;
            targetRect.h = 54;

            SDL_RenderCopy(splash.renderer, t, nullptr, &targetRect);

            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }

        SDL_RenderPresent(splash.renderer);
    }

    SplashWindow createSplashWindow(bool small) {
        SplashWindow splash;

        int nRenderDrivers = SDL_GetNumRenderDrivers();
        int driverIdx = -1;

        for (int i = 0; i < nRenderDrivers; i++) {
            SDL_RendererInfo inf;
            SDL_GetRenderDriverInfo(i, &inf);

            logMsg("Render driver: %s", inf.name);

#ifdef _WIN32
            if (strcmp(inf.name, "direct3d11") == 0)
                driverIdx = i;
#endif
        }

        splash.win = SDL_CreateWindow("Loading...",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            small ? 460 : 800, small ? 215 : 600,
            SDL_WINDOW_BORDERLESS | SDL_WINDOW_SKIP_TASKBAR
        );

        if (splash.win == nullptr) {
            fatalErr("Failed to create splash screen");
        }

        splash.renderer = SDL_CreateRenderer(splash.win, driverIdx, SDL_RENDERER_ACCELERATED);

        if (splash.renderer == nullptr) {
            fatalErr("Failed to create splash screen renderer");
        }

        SDL_RaiseWindow(splash.win);

        splash.bgSurface = loadDataFileToSurface(small ? "splash_game.png" : "splash.png");
        splash.bgTexture = SDL_CreateTextureFromSurface(splash.renderer, splash.bgSurface);
        setWindowIcon(splash.win);

        redrawSplashWindow(splash, "");

        return splash;
    }
}
