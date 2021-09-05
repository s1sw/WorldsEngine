#pragma once
#include "ISplashScreen.hpp"

struct HWND__;
typedef struct HWND__* HWND;
typedef unsigned long long WPARAM;
typedef long long LPARAM;
typedef __int64 LONG_PTR;
typedef LONG_PTR LRESULT;

namespace worlds {
    class SplashScreenImplWin32 : public ISplashScreen {
    public:
        SplashScreenImplWin32(bool small);
        void changeOverlay(const char*) override;
        ~SplashScreenImplWin32();
    private:
        void eventLoop();
        // Avoid pulling in windows.h
        // Yes this file is only included in one place but that
        // file already has so many includes it's ridiculous
        struct State;
        static State* s;

        static LRESULT WndProc(HWND, unsigned int, WPARAM, LPARAM);
    };
}