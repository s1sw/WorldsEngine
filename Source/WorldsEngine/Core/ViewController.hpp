#pragma once
#include <stdint.h>

namespace worlds
{
    struct EngineInterfaces;
    class RTTPass;
    class Window;

    struct MainViewSettings
    {
        bool xrEnabled;
        Window* boundWindow;
    };

    // Controls the main view (RTTPass rendering to the screen or HMD) when running standalone.
    class ViewController
    {
    public:
        ViewController(const EngineInterfaces& interfaces, MainViewSettings settings);
        ~ViewController();
        void draw();
    private:
        void getViewResolution(uint32_t* width, uint32_t* height);
        void resizeIfNecessary();
        const EngineInterfaces& interfaces;
        RTTPass* rttPass;
        bool xrEnabled;
        Window* boundWindow;
    };
}