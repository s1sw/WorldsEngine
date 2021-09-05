#ifdef _WIN32
#include "SplashScreenWin32.hpp"

#include <Windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <thread>
#include <dwmapi.h>
#include <SDL_filesystem.h>
#include <stb_image.h>
#include <Core/Log.hpp>
#undef small


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
        HBITMAP backgroundBitmap;
        HDC dc;
    };

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

    HBITMAP createBitmapFromFile(const char* fname, HDC dc) {
        char* basePath = SDL_GetBasePath();

        char* buf = (char*)alloca(strlen(fname) + strlen(basePath) + 2);
        buf[0] = 0;
        strcat(buf, basePath);
        strcat(buf, "EngineData/");
        strcat(buf, fname);

        int imgWidth, imgHeight, channels;
        unsigned char* imgData = stbi_load(buf, &imgWidth, &imgHeight, &channels, 4);
        SDL_free(basePath);

        BITMAPINFO bminfo = { 0 };
        bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bminfo.bmiHeader.biWidth = imgWidth;
        bminfo.bmiHeader.biHeight = -((LONG)imgHeight);
        bminfo.bmiHeader.biPlanes = 1;
        bminfo.bmiHeader.biBitCount = 32;
        bminfo.bmiHeader.biCompression = BI_RGB;

        void* imageBits = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &bminfo, DIB_RGB_COLORS, &imageBits, NULL, 0);

        memcpy(imageBits, imgData, imgWidth * imgHeight * 4);
        stbi_image_free(imgData);

        return bitmap;
    }

    SplashScreenImplWin32::SplashScreenImplWin32(bool small) {
        s = new State;
        
        s->t = new std::thread([this, small] {
            const char* CLASS_NAME = "WorldsSplashScreen";

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
            s->dc = CreateCompatibleDC(winDc);
            s->backgroundBitmap = createBitmapFromFile("splash.png", s->dc);
            SelectBitmap(s->dc, s->backgroundBitmap);

            ShowWindow(s->hwnd, SW_SHOW);
            checkWinErr();
            eventLoop();
            });
    }

    void SplashScreenImplWin32::changeOverlay(const char* overlay) {

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
        static State* state = nullptr;
        switch (msg) {
        case WM_NCCREATE: {
            state = (State*)(((CREATESTRUCT*)lParam)->lpCreateParams);
            break;
        }
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
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HDC hdcMem = CreateCompatibleDC(hdc);
            SelectObject(hdcMem, state->backgroundBitmap);

            BitBlt(hdc, 0, 0, 800, 600, hdcMem, 0, 0, SRCCOPY);

            DeleteDC(hdcMem);
            EndPaint(hwnd, &ps);
            checkWinErr();
            return 0;
        }
        }

        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}
#endif