#pragma once
#include "ISplashScreen.hpp"
#include <stdint.h>
#include <xcb/xcb.h>

namespace std
{
    class thread;
}

namespace worlds
{
    class SplashScreenImplX11 : public ISplashScreen
    {
      public:
        SplashScreenImplX11(bool small);
        void changeOverlay(const char *) override;
        ~SplashScreenImplX11();

      private:
        volatile bool running = true;
        bool small;
        void handleEvent(xcb_generic_event_t *event);
        void eventLoop();
        xcb_window_t window;
        xcb_connection_t *connection;
        xcb_gcontext_t graphicsContext;
        xcb_pixmap_t background;
        std::thread *thread;
    };
}
