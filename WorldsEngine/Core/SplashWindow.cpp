#include "SplashWindow.hpp"
#include <SDL_events.h>
#include "Fatal.hpp"
#include "EarlySDLUtil.hpp"
#include "Log.hpp"
#include "SDL_timer.h"
#include <functional>
#include <thread>

namespace worlds {
    SplashWindow::SplashWindow(bool small)
        : small { small }
        , overlaySurface { nullptr }
        , overlayTexture { nullptr }
        , overlay {}
        , loadedOverlay {} {
        winThread = std::thread{std::bind(&SplashWindow::eventThread, this)};

        while (!windowCreated) {}
    }

    void SplashWindow::changeOverlay(std::string overlay) {
        this->overlay = overlay;
    }

    SplashWindow::~SplashWindow() {
        // stop the event thread
        running = false;
        winThread.join();

        if (overlaySurface) {
            SDL_DestroyTexture(overlayTexture);
            void* overlaySurfData = overlaySurface->pixels;
            SDL_FreeSurface(overlaySurface);
            free(overlaySurfData);
        }

        void* dataPtr = bgSurface->pixels;

        SDL_DestroyTexture(bgTexture);
        SDL_DestroyRenderer(renderer);

        SDL_FreeSurface(bgSurface);
        free(dataPtr);

        SDL_DestroyWindow(win);
    }

    void SplashWindow::redraw() {
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, bgTexture, nullptr, nullptr);

        if (overlayTexture) {
            SDL_Rect targetRect;
            targetRect.x = 544;
            targetRect.y = 546;
            targetRect.w = 256;
            targetRect.h = 54;

            SDL_RenderCopy(renderer, overlayTexture, nullptr, &targetRect);
        }

        SDL_RenderPresent(renderer);
    }

    void SplashWindow::eventThread() {
        win = SDL_CreateWindow("Loading...",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            small ? 460 : 800, small ? 215 : 600,
            SDL_WINDOW_BORDERLESS | SDL_WINDOW_SKIP_TASKBAR
        );

        if (win == nullptr) {
            fatalErr("Failed to create splash screen");
        }

        renderer = SDL_CreateRenderer(win, 0, 0);

        if (renderer == nullptr) {
            fatalErr("Failed to create splash screen renderer");
        }

        SDL_RaiseWindow(win);

        windowCreated = true;

        bgSurface = loadDataFileToSurface(small ? "splash_game.png" : "splash.png");
        bgTexture = SDL_CreateTextureFromSurface(renderer, bgSurface);
        setWindowIcon(win, "icon_engine.png");

        while (running) {
            if (loadedOverlay != overlay) {
                if (!overlay.empty()) {
                    if (overlaySurface) {
                        SDL_DestroyTexture(overlayTexture);

                        void* overlaySurfData = overlaySurface->pixels;
                        SDL_FreeSurface(overlaySurface);
                        free(overlaySurfData);
                    }

                    overlaySurface = loadDataFileToSurface("SplashText/" + overlay + ".png");
                    overlayTexture = SDL_CreateTextureFromSurface(renderer, overlaySurface);
                } else {
                    overlaySurface = nullptr;
                    overlayTexture = nullptr;
                }

                loadedOverlay = overlay;
            }

            redraw();

            //SDL_Event evt;
            //SDL_Delay(10);
        }

    }
}
