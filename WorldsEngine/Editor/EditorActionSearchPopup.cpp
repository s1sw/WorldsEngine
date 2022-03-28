#include "EditorActionSearchPopup.hpp"
#include "Editor/EditorActions.hpp"
#include "Editor.hpp"
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>
#include "SDL_scancode.h"

namespace worlds {
    EditorActionSearchPopup::EditorActionSearchPopup(Editor* ed, entt::registry& reg) 
        : ed(ed)
        , reg(reg) {}

    void EditorActionSearchPopup::show() {
        ImGui::OpenPopup("Action Search");
    }

    void EditorActionSearchPopup::hide() {
        // TODO
    }

    void EditorActionSearchPopup::draw() {
        ImVec2 size(500.0f, ImGui::GetTextLineHeightWithSpacing() * (2 + currentCandidateList.numElements()));
        ImVec2 pos(ImGui::GetMainViewport()->Size.x * 0.5f, 200.0f);
        pos = ImVec2(pos.x - size.x * 0.5f, pos.y);
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);

        if (ImGui::BeginPopup("Action Search")) {
            ImGui::PushItemWidth(size.x);
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##search", &currentSearchText)) {
                currentCandidateList = EditorActions::searchForActions(currentSearchText);
            }

            if (ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_DOWN] == 0.0f) {
                selectedIndex++;
            }

            if (ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_UP] == 0.0f) {
                selectedIndex--;
            }

            if (ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_ESCAPE] == 0.0f) {
                ImGui::CloseCurrentPopup();
            }

            if (selectedIndex >= currentCandidateList.numElements()) selectedIndex = 0;
            if (selectedIndex < 0) selectedIndex = currentCandidateList.numElements() - 1;

            float lineHeight = ImGui::CalcTextSize("w").y;
            int idx = 0;
            for (uint32_t candidate : currentCandidateList) {
                const EditorAction& action = EditorActions::getActionByHash(candidate);

                ImVec2 cursorPos = ImGui::GetCursorPos();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                float windowWidth = ImGui::GetWindowWidth();
                ImVec2 windowPos = ImGui::GetWindowPos();

                if (selectedIndex == idx) { 
                    drawList->AddRectFilled(
                        ImVec2(0.0f + windowPos.x, cursorPos.y + windowPos.y - ImGui::GetScrollY()),
                        ImVec2(windowWidth + windowPos.x, cursorPos.y + lineHeight + windowPos.y - ImGui::GetScrollY()),
                        ImColor(0, 75, 150)
                    );

                    if (ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_RETURN] == 0.0f) {
                        ImGui::CloseCurrentPopup();
                        currentCandidateList.clear();
                        currentSearchText.clear();
                        action.function(ed, reg);
                    }
                }

                ImGui::Text("%s", action.friendlyString.cStr());

                idx++;
            }
            ImGui::EndPopup();
        }
    }
}