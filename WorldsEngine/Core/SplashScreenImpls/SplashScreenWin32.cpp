#ifdef _WIN32
#include "SplashScreenWin32.hpp"

#define WIN32_LEAN_AND_MIN
#include <Windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <thread>
#include <dwmapi.h>
#include <SDL_filesystem.h>
#include <stb_image.h>
#include <Core/Log.hpp>
#include <mutex>

// ew ew ew ew ew
#define min(a, b) a < b ? a : b
#define max(a, b) a > b ? a : b
#include <gdiplus.h>
#undef small
#undef min
#undef max

namespace {
    // we cannot just use WS_POPUP style
    // WS_THICKFRAME: without this the window cannot be resized and so aero snap, de-maximizing and minimizing won't work
    // WS_SYSMENU: enables the context menu with the move, close, maximize, minize... commands (shift + right-click on the task bar item)
    // WS_CAPTION: enables aero minimize animation/transition
    // WS_MAXIMIZEBOX, WS_MINIMIZEBOX: enable minimize/maximize
    enum class Style : DWORD {
        windowed = WS_OVERLAPPEDWINDOW | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        aero_borderless = WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
        basic_borderless = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX
    };

    auto composition_enabled() -> bool {
        BOOL composition_enabled = FALSE;
        bool success = ::DwmIsCompositionEnabled(&composition_enabled) == S_OK;
        return composition_enabled && success;
    }

    auto select_borderless_style() -> Style {
        return composition_enabled() ? Style::aero_borderless : Style::basic_borderless;
    }
}

namespace worlds {
    struct SplashScreenImplWin32::State {
        HWND hwnd;
        std::thread* t;
        Gdiplus::Image* background;
        Gdiplus::Image* foreground = nullptr;
        std::mutex mutex;
    };

    SplashScreenImplWin32::State* SplashScreenImplWin32::s;

    void checkWinErr() {
        auto err = GetLastError();

        if (err != 0) {
            LPSTR messageBuffer = nullptr;

            //Ask Win32 to give us the string version of that message ID.
            //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
            size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

            logErr("Win32 error: %s", messageBuffer);

            LocalFree(messageBuffer);
        }
    }

    Gdiplus::Image* createImageFromFile(const char* fname) {
        char* basePath = SDL_GetBasePath();

        char* buf = (char*)alloca(strlen(fname) + strlen(basePath) + 12 + 1);
        buf[0] = 0;
        strcat(buf, basePath);
        strcat(buf, "EngineData/");
        strcat(buf, fname);

        SDL_free(basePath);

        wchar_t* wbuf = (wchar_t*)alloca((strlen(fname) + strlen(basePath) + 12 + 1) * sizeof(wchar_t));

        mbstowcs(wbuf, buf, (strlen(fname) + strlen(basePath) + 12 + 1) * sizeof(wchar_t));

        Gdiplus::Image* img = new Gdiplus::Image(wbuf);

        return img;
    }

    SplashScreenImplWin32::SplashScreenImplWin32(bool small) {
        s = new State;
        
        s->t = new std::thread([this, small] {
            const char* CLASS_NAME = "WorldsSplashScreen";

            Gdiplus::GdiplusStartupInput gdiplusStartupInput;
            ULONG_PTR gdiplusToken;
            Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

            WNDCLASSEX wcx{};
            wcx.cbSize = sizeof(wcx);
            wcx.style = CS_HREDRAW | CS_VREDRAW;
            wcx.hInstance = nullptr;
            wcx.lpfnWndProc = WndProc;
            wcx.lpszClassName = CLASS_NAME;
            wcx.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
            wcx.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
            const ATOM result = ::RegisterClassEx(&wcx);

            int width = small ? 460 : 800;
            int height = small ? 215 : 600;

            RECT clientRect;
            GetClientRect(GetDesktopWindow(), &clientRect);
            clientRect.left = (clientRect.right / 2) - (width / 2);
            clientRect.top = (clientRect.bottom / 2) - (height / 2);

            s->hwnd = CreateWindowEx(
                0, CLASS_NAME, "Loading...",
                static_cast<DWORD>(Style::aero_borderless), clientRect.left, clientRect.top,
                width, height, nullptr, nullptr, nullptr, this
            );

            Style new_style = (true) ? select_borderless_style() : Style::windowed;
            Style old_style = static_cast<Style>(GetWindowLongPtr(s->hwnd, GWL_STYLE));

            if (composition_enabled()) {
                static const MARGINS shadow_state{ 1,1,1,1 };
                DwmExtendFrameIntoClientArea(s->hwnd, &shadow_state);
            }

            HDC winDc = GetDC(s->hwnd);
            s->background = createImageFromFile("splash.png");

            ShowWindow(s->hwnd, SW_SHOW);
            checkWinErr();
            eventLoop();

            Gdiplus::GdiplusShutdown(gdiplusToken);
            });
    }

    void SplashScreenImplWin32::changeOverlay(const char* overlay) {
        s->mutex.lock();

        char* basePath = SDL_GetBasePath();

        const char* splashTextDir = "EngineData/SplashText/";
        const char* ext = ".png";

        char* buf = (char*)alloca(strlen(overlay) + strlen(basePath) + strlen(splashTextDir) + strlen(ext) + 1);
        buf[0] = 0;
        strcat(buf, basePath);
        strcat(buf, splashTextDir);
        strcat(buf, overlay);
        strcat(buf, ext);

        SDL_free(basePath);

        wchar_t* wbuf = (wchar_t*)alloca((strlen(overlay) + strlen(basePath) + strlen(splashTextDir) + strlen(ext) + 1) * sizeof(wchar_t));

        mbstowcs(wbuf, buf, (strlen(overlay) + strlen(basePath) + strlen(splashTextDir) + strlen(ext) + 1) * sizeof(wchar_t));

        s->foreground = new Gdiplus::Image(wbuf);
        RedrawWindow(s->hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_INTERNALPAINT);
        s->mutex.unlock();
    }

    void SplashScreenImplWin32::eventLoop() {
        State* statePtr = s;
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) == TRUE) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        delete statePtr;
    }

    SplashScreenImplWin32::~SplashScreenImplWin32() {
        PostMessage(s->hwnd, WM_USER, 0, 0);
    }

    LRESULT hit_test(HWND hwnd, POINT cursor) {
        // identify borders and corners to allow resizing the window.
        // Note: On Windows 10, windows behave differently and
        // allow resizing outside the visible window frame.
        // This implementation does not replicate that behavior.
        const POINT border{
            ::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER),
            ::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)
        };
        RECT window;
        if (!::GetWindowRect(hwnd, &window)) {
            return HTNOWHERE;
        }

        return HTCAPTION;
    }

    LRESULT SplashScreenImplWin32::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_NCCALCSIZE: {
            return 0;
            break;
        }
        case WM_NCHITTEST: {
            // When we have no border or title bar, we need to perform our
            // own hit testing to allow resizing and moving
            return hit_test(hwnd, POINT{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
                });
            break;
        }
        case WM_NCACTIVATE: {
            if (!composition_enabled()) {
                // Prevents window frame reappearing on window activation
                // in "basic" theme, where no aero shadow is present.
                return 1;
            }
            break;
        }

        case WM_CLOSE: {
            DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
        case WM_USER: {
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_PAINT: {
            s->mutex.lock();
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HDC hdcMem = CreateCompatibleDC(hdc);

            Gdiplus::Rect rect{ 0, 0, 800, 600 };
            Gdiplus::Rect overlayRect{ 544, 546, 256, 54 };

            Gdiplus::Graphics g(hdc);
            g.DrawImage(s->background, rect);

            if (s->foreground) {
                g.DrawImage(s->foreground, overlayRect);
            }

            EndPaint(hwnd, &ps);
            checkWinErr();
            s->mutex.unlock();
            return 0;
        }
        }

        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}
#endif