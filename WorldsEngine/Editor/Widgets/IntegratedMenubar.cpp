#include "IntegratedMenubar.hpp"
#include <Core/ConVar.hpp>
#include <Core/Engine.hpp>
#include <Core/Window.hpp>
#include <Editor/Editor.hpp>
#include <ImGui/imgui.h>
#ifdef _WIN32
#include <SDL_syswm.h>
#include <dwmapi.h>
#include <slib/Win32Util.hpp>
#endif

namespace worlds
{
    static ConVar integratedMenuBar{"ed_integratedMenuBar", "0"};
    IntegratedMenubar::IntegratedMenubar(EngineInterfaces interfaces) : interfaces(interfaces)
    {
    }

    void IntegratedMenubar::draw()
    {
        const char* windowTitle = interfaces.engine->getMainWindow().getTitle();
        glm::vec2 textSize = ImGui::CalcTextSize(windowTitle);
        glm::vec2 menuBarCenter = ImGui::GetWindowSize();
        menuBarCenter *= 0.5f;

        glm::vec2 vpOffset = ImGui::GetMainViewport()->Pos;

        Window& mainWindow = interfaces.engine->getMainWindow();

        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(mainWindow.getWrappedHandle(), &wmInfo);

        HWND hwnd = wmInfo.info.win.window;

        static bool extendedFrame = false;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (integratedMenuBar.getInt())
        {
            drawList->AddText(vpOffset +
                                  glm::vec2(menuBarCenter.x - (textSize.x * 0.5f), ImGui::GetWindowHeight() * 0.15f),
                              ImColor(255, 255, 255), windowTitle);

            SDL_SetWindowBordered(interfaces.engine->getMainWindow().getWrappedHandle(), SDL_FALSE);
            float barWidth = ImGui::GetWindowWidth();
            float barHeight = ImGui::GetWindowHeight();
            const float crossSize = 6.0f;
            glm::vec2 crossCenter(ImGui::GetWindowWidth() - 17.0f - crossSize, menuBarCenter.y);
            crossCenter += (glm::vec2)vpOffset;
            crossCenter -= glm::vec2(0.5f, 0.5f);
            auto crossColor = ImColor(255, 255, 255);

            ImVec2 mousePos = ImGui::GetMousePos();
            mousePos.x -= vpOffset.x;
            mousePos.y -= vpOffset.y;

            if (mousePos.x > barWidth - 45.0f && mousePos.y < barHeight && ImGui::IsWindowHovered())
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    interfaces.engine->quit();
                }
                drawList->AddRectFilled(vpOffset + glm::vec2(barWidth - 45.0f, 0.0f),
                                        vpOffset + glm::vec2(barWidth, barHeight), ImColor(255, 0, 0, 255));
            }

            drawList->AddLine(crossCenter + glm::vec2(+crossSize, +crossSize),
                              crossCenter + glm::vec2(-crossSize, -crossSize), crossColor, 1.0f);
            drawList->AddLine(crossCenter + glm::vec2(+crossSize, -crossSize),
                              crossCenter + glm::vec2(-crossSize, +crossSize), crossColor, 1.0f);

            Window& window = interfaces.engine->getMainWindow();
            glm::vec2 maximiseCenter(barWidth - 45.0f - 22.0f, menuBarCenter.y);
            maximiseCenter += vpOffset;
            if (mousePos.x > barWidth - 90.0f && mousePos.x < barWidth - 45.0f && mousePos.y < barHeight &&
                ImGui::IsWindowHovered())
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    if (window.isMaximised())
                        window.restore();
                    else
                        window.maximise();
                }
                drawList->AddRectFilled(vpOffset + glm::vec2(barWidth - 45.0f - 45.0f, 0.0f),
                                        vpOffset + glm::vec2(barWidth - 45.0f, barHeight), ImColor(255, 255, 255, 50));
            }

            if (!window.isMaximised())
            {
                drawList->AddRect(maximiseCenter - glm::vec2(crossSize, crossSize),
                                  maximiseCenter + glm::vec2(crossSize, crossSize), ImColor(255, 255, 255));
            }
            else
            {
                drawList->AddRect(maximiseCenter - glm::vec2(crossSize - 3, crossSize),
                                  maximiseCenter + glm::vec2(crossSize, crossSize - 3), ImColor(255, 255, 255));

                drawList->AddRectFilled(maximiseCenter - glm::vec2(crossSize, crossSize - 3),
                                        maximiseCenter + glm::vec2(crossSize - 3, crossSize),
                                        ImGui::GetColorU32(ImGuiCol_MenuBarBg));

                drawList->AddRect(maximiseCenter - glm::vec2(crossSize, crossSize - 3),
                                  maximiseCenter + glm::vec2(crossSize - 3, crossSize), ImColor(255, 255, 255));
            }

            glm::vec2 minimiseCenter(barWidth - 90.0f - 22.0f, menuBarCenter.y);
            minimiseCenter += vpOffset;
            if (mousePos.x > barWidth - 135.0f && mousePos.x < barWidth - 90.0f && mousePos.y < barHeight &&
                ImGui::IsWindowHovered())
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {

                    if (window.isMinimised())
                        window.restore();
                    else
                        window.minimise();
                }
                drawList->AddRectFilled(vpOffset + glm::vec2(barWidth - 135.0f, 0.0f),
                                        vpOffset + glm::vec2(barWidth - 90.0f, barHeight), ImColor(255, 255, 255, 50));
            }
            drawList->AddRectFilled(minimiseCenter - glm::vec2(5, 0), minimiseCenter + glm::vec2(5, 1),
                                    ImColor(255, 255, 255));

#ifdef _WIN32
            glm::vec2 ws{windowSize};
            ws += vpOffset;
            uint8_t borderR;
            uint8_t borderG;
            uint8_t borderB;
            if (interfaces.editor->getCurrentState() == GameState::Editing)
            {
                // if (!interfaces.engine->getMainWindow().isMaximised()) {
                uint8_t r, g, b;
                slib::Win32Util::getAccentColor(r, g, b);
                if (interfaces.engine->getMainWindow().isFocused())
                {
                    borderR = r;
                    borderG = g;
                    borderB = b;
                    ImGui::GetForegroundDrawList()->AddRect(vpOffset, ws, ImColor(r, g, b));
                }
                else
                {
                    borderR = 90;
                    borderG = 90;
                    borderB = 90;
                }
                //}
            }
            else
            {
                if (interfaces.editor->getCurrentState() == GameState::Playing)
                {
                    borderR = 50;
                    borderG = 127;
                    borderB = 50;
                }
                else
                {
                    borderR = 127;
                    borderG = 50;
                    borderB = 50;
                }
            }

            ImGui::GetForegroundDrawList()->AddRect(vpOffset, ws, ImColor(borderR, borderG, borderB));

            COLORREF border = RGB(borderR, borderG, borderB);

            static bool supportsBorderColor = true;

            const DWORD DWMWA_BORDER_COLOR = 34;
            // if (supportsBorderColor) {
            //    HRESULT result = DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
            //    if (result == E_INVALIDARG) {
            //        supportsBorderColor = false;
            //    }
            //}

            if (!mainWindow.isMaximised() && !extendedFrame)
            {
                MARGINS m{-1};
                DwmExtendFrameIntoClientArea(hwnd, &m);
                extendedFrame = true;
            }
#endif
        }
        else
        {
            SDL_SetWindowBordered(interfaces.engine->getMainWindow().getWrappedHandle(), SDL_TRUE);
            extendedFrame = false;
            MARGINS m{};
            DwmExtendFrameIntoClientArea(hwnd, &m);
        }
    }
}