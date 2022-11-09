#pragma once
#include <string>
#include <ImGui/imgui.h>

namespace worlds
{
    struct EngineInterfaces;
    struct DebugTimeInfo
    {
        double deltaTime;
        double updateTime;
        double simTime;
        bool didSimRun;
        double lastUpdateTime;
        double lastTickRendererTime;
        int frameCounter;
    };

    class Window;
    ImFont* addImGuiFont(std::string fontPath, float size, ImFontConfig* config = nullptr,
                         const ImWchar* ranges = nullptr);
    void setupUIFonts(float dpiScale);
    void initializeImGui(Window* mainWindow, bool isEditor, bool isHeadless);

    void drawDebugInfoWindow(const EngineInterfaces& interfaces, DebugTimeInfo debugTimeInfo);
}