#include "EditorWindows.hpp"
#include "../../ImGui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../../ImGui/imgui_internal.h"
#include "../../Audio/Audio.hpp"

namespace worlds {
    ImTextureID bgId = nullptr;
    ImTextureID bradnoId = nullptr;

    void AboutWindow::setActive(bool active) {
        this->active = active;
        bgId = editor->texManager()->loadOrGet(g_assetDB.addOrGetExisting("UI/Images/worlds_no_logo.png"));
        bradnoId = editor->texManager()->loadOrGet(g_assetDB.addOrGetExisting("UI/Images/bradno.png"));
    }

    ImVec2 rotatePoint(ImVec2 p, float angle) {
        float s = sin(angle);
        float c = cos(angle);

        float xnew = p.x * c - p.y * s;
        float ynew = p.x * s + p.y * c;

        return ImVec2(xnew, ynew);
    }

    const SDL_Scancode bradnoCode[] = {
        SDL_SCANCODE_B,
        SDL_SCANCODE_R,
        SDL_SCANCODE_A,
        SDL_SCANCODE_D,
        SDL_SCANCODE_N,
        SDL_SCANCODE_O
    };

    int bradnoPosition = 0;
    float lastBradnoTime = 0.0f;
    bool showBradno = false;

    void AboutWindow::draw(entt::registry&) {
        //ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        if (ImGui::Begin("About", &active)) {
            if (ImGui::GetTime() - lastBradnoTime > 7.0f) {
                bradnoPosition = 0;
                showBradno = false;
            }

            if (ImGui::GetIO().KeysDownDuration[bradnoCode[bradnoPosition]] == 0.0f) {
                lastBradnoTime = ImGui::GetTime();
                bradnoPosition++;
                if (bradnoPosition >= (int)sizeof(bradnoCode) / (int)sizeof(bradnoCode[0])) {
                    showBradno = true;
                    AudioSystem::getInstance()->playOneShotClip(g_assetDB.addOrGetExisting("Audio/SFX/smooch.ogg"), glm::vec3{0.0f}, false, 0.4f);
                    bradnoPosition = 0;
                }
            }

            auto screenCursorPos = ImGui::GetCursorPos() + ImGui::GetWindowPos() - ImVec2(0.0f, ImGui::GetScrollY());
            auto corner = screenCursorPos + ImVec2(494, 174);
            auto* drawList = ImGui::GetWindowDrawList();

            drawList->AddImage(bgId, screenCursorPos, corner);

            // draw orbit circles
            auto center = screenCursorPos + ImVec2(174, 60);
            drawList->AddCircleFilled(center, 6.0f, ImColor(1.0f, 1.0f, 1.0f), 24);
            drawList->AddCircle(center, 20.0f, ImColor(1.0f, 1.0f, 1.0f), 32, 3.0f);
            drawList->AddCircle(center, 35.0f, ImColor(1.0f, 1.0f, 1.0f), 32, 3.0f);
            double currTime = ImGui::GetTime();
            currTime *= 0.25f;

            // draw planets
            auto innerPos = center + ImVec2(glm::sin(currTime), glm::cos(currTime)) * 20.0f;
            drawList->AddCircleFilled(innerPos, 4.0f, ImColor(1.0f, 1.0f, 1.0f), 32);

            currTime *= 1.25f;

            auto outerPos = center + ImVec2(glm::sin(currTime), glm::cos(currTime)) * 35.0f;
            drawList->AddCircleFilled(outerPos, 4.0f, ImColor(1.0f, 1.0f, 1.0f), 32);

            ImGui::SetCursorPos(ImGui::GetCursorStartPos() + ImVec2(0, 174 + 5));
            ImGui::Text("Programmed by Someone Somewhere :)");
            ImGui::Text("Open source libraries:");
            ImGui::Text(" - EnTT");
            ImGui::Text(" - PhysX");
            ImGui::Text(" - Dear ImGUI");
            ImGui::Text(" - stb_image");
            ImGui::Text(" - stb_vorbis");
            ImGui::Text(" - sajson");
            ImGui::Text(" - crunch");
            ImGui::Text(" - VulkanMemoryAllocator");
            ImGui::Text(" - moodycamel readerwriterqueue");
            ImGui::Text(" - tinyobjloader");
            ImGui::Text(" - MikkTSpace");
            ImGui::Text(" - SDL2");
            ImGui::Text(" - Wren");
            ImGui::Text(" - ENet");
            ImGui::Text(" - PhysFS");
            ImGui::Text(" - Tracy Profiler");
            ImGui::Text(" - Vookoo (sorta, it's been rewritten by now :P)");

            auto cursorX = ImGui::GetCursorStartPos().x + 375;

            ImGui::SetCursorPos(ImGui::GetCursorStartPos() + ImVec2(375, 174 + 5 + ImGui::GetTextLineHeightWithSpacing()));
            ImGui::Text("Thanks to:");

            if (showBradno) {
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - lil b (based god)");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - soulja boy");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - boneworks ninja");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - VR_Player");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - bradno (smooch)");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - doctor SEX 😳");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - L4rs the gamer girl for the sanity depriving C++ questions");
            } else {
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - PixHammer for the physics hands help");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - SLZ for making the game that started this mess");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - Maranara for");
                ImGui::SetCursorPosX(cursorX);
                ImGui::Text(" - Tabloid for motivation");
            }

            ImGui::SetCursorPosX(cursorX);
            auto bradnoPos = ImGui::GetCursorPos() + ImGui::GetWindowPos() + ImVec2(0, 100);
            const int BRADNO_HALF_WIDTH = 145;
            const int BRADNO_HALF_HEIGHT = 115;

            float angle = currTime * 2.0f;

            auto bradnoCenter = bradnoPos + (ImVec2(290, 230) / 2);
            auto p1 = bradnoCenter + rotatePoint(ImVec2(-BRADNO_HALF_WIDTH, -BRADNO_HALF_HEIGHT), angle);
            auto p2 = bradnoCenter + rotatePoint(ImVec2(BRADNO_HALF_WIDTH, -BRADNO_HALF_HEIGHT), angle);
            auto p3 = bradnoCenter + rotatePoint(ImVec2(BRADNO_HALF_WIDTH, BRADNO_HALF_HEIGHT), angle);
            auto p4 = bradnoCenter + rotatePoint(ImVec2(-BRADNO_HALF_WIDTH, BRADNO_HALF_HEIGHT), angle);

            if (showBradno)
                drawList->AddImageQuad(bradnoId, p1, p2, p3, p4);

        }
        ImGui::End();
        //ImGui::PopStyleColor();
    }
}
