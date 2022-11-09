#include "EngineInternal.hpp"
#include "Window.hpp"
#include <Core/Log.hpp>
#include <physfs.h>
#include <Libs/IconsFontAwesome5.h>
#include <Libs/IconsFontaudio.h>
#include <ImGui/imgui_impl_sdl.h>

namespace worlds
{
    extern ImFont* boldFont;

    ImFont* addImGuiFont(std::string fontPath, float size, ImFontConfig* config, const ImWchar* ranges)
    {
        PHYSFS_File* ttfFile = PHYSFS_openRead(fontPath.c_str());
        if (ttfFile == nullptr)
        {
            logWarn("Couldn't open font file");
            return nullptr;
        }

        auto fileLength = PHYSFS_fileLength(ttfFile);

        if (fileLength == -1)
        {
            PHYSFS_close(ttfFile);
            logWarn("Couldn't determine size of editor font file");
            return nullptr;
        }

        void* buf = malloc(fileLength);
        auto readBytes = PHYSFS_readBytes(ttfFile, buf, fileLength);

        if (readBytes != fileLength)
        {
            PHYSFS_close(ttfFile);
            logWarn("Failed to read full TTF file");
            return nullptr;
        }

        PHYSFS_close(ttfFile);

        ImFontConfig defaultConfig{};

        if (config)
        {
            memcpy(config->Name, fontPath.c_str(), fontPath.size());
        }
        else
        {
            for (size_t i = 0; i < fontPath.size(); i++)
            {
                defaultConfig.Name[i] = fontPath[i];
            }
            config = &defaultConfig;
        }

        return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(buf, (int)readBytes, size, config, ranges);
    }

    void setupUIFonts(float dpiScale)
    {
        if (PHYSFS_exists("Fonts/EditorFont.ttf"))
            ImGui::GetIO().Fonts->Clear();

        boldFont = addImGuiFont("Fonts/EditorFont-Bold.ttf", 20.0f * dpiScale);
        ImGui::GetIO().FontDefault = addImGuiFont("Fonts/EditorFont.ttf", 20.0f * dpiScale);

        static const ImWchar iconRanges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
        ImFontConfig iconConfig{};
        iconConfig.MergeMode = true;
        iconConfig.PixelSnapH = true;
        iconConfig.OversampleH = 1;

        addImGuiFont("Fonts/" FONT_ICON_FILE_NAME_FAR, 17.0f * dpiScale, &iconConfig, iconRanges);
        addImGuiFont("Fonts/" FONT_ICON_FILE_NAME_FAS, 17.0f * dpiScale, &iconConfig, iconRanges);

        ImFontConfig iconConfig2{};
        iconConfig2.MergeMode = true;
        iconConfig2.PixelSnapH = true;
        iconConfig2.OversampleH = 2;
        iconConfig2.GlyphOffset = ImVec2(-3.0f, 5.0f);
        iconConfig2.GlyphExtraSpacing = ImVec2(-5.0f, 0.0f);

        static const ImWchar iconRangesFAD[] = {ICON_MIN_FAD, ICON_MAX_FAD, 0};

        addImGuiFont("Fonts/" FONT_ICON_FILE_NAME_FAD, 22.0f * dpiScale, &iconConfig2, iconRangesFAD);
    }

    extern void loadDefaultUITheme();

    void initializeImGui(Window* mainWindow, bool isEditor, bool headless)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
        io.IniFilename = isEditor ? "imgui_editor.ini" : "imgui.ini";
        io.Fonts->TexDesiredWidth = 512;

        if (!headless)
        {
            ImGui_ImplSDL2_InitForVulkan(mainWindow->getWrappedHandle());

            float dpiScale = 1.0f;
            int dI = SDL_GetWindowDisplayIndex(mainWindow->getWrappedHandle());
            float ddpi, hdpi, vdpi;
            if (SDL_GetDisplayDPI(dI, &ddpi, &hdpi, &vdpi) != -1)
            {
                dpiScale = ddpi / 96.0f;
            }

            setupUIFonts(dpiScale);

            loadDefaultUITheme();
        }
    }
}