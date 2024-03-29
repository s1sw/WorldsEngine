#include "SearchPopup.hpp"
#include <ImGui/imgui_internal.h>

namespace worlds {
    template<typename T>
    void SearchPopup<T>::drawPopup(const char* title) {
        ImVec2 size(500.0f, ImGui::GetTextLineHeightWithSpacing() * (3 + std::min(currentCandidateList.numElements(), (size_t)10)) + 5.0f);

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGuiContext& g = *GImGui;
        
        if (g.NavWindow != nullptr) {
            vp = g.NavWindow->Viewport;
        }

        ImVec2 pos(vp->Size.x * 0.5f, 200.0f);
        pos = ImVec2(pos.x - size.x * 0.5f, pos.y);
        pos.x += vp->Pos.x;
        pos.y += vp->Pos.y;

        ImGui::SetNextWindowViewport(vp->ID);
        //ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);

        bool activate = false;
        size_t activateIndex;

        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fadeAlpha);
        if (ImGui::BeginPopup(title)) {
            pushBoldFont();
            EditorUI::centeredText(title);
            ImGui::PopFont();

            ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth());
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##Asset", &currentSearchText)) {
                updateCandidates();
            }

            bool setScroll = false;

            if (ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_DOWN] == 0.0f) {
                selectedIndex++;
                setScroll = true;
            }

            if (ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_UP] == 0.0f) {
                selectedIndex--;
                setScroll = true;
            }

            if (ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_ESCAPE] == 0.0f) {
                ImGui::CloseCurrentPopup();
            }

            if (selectedIndex >= currentCandidateList.numElements()) selectedIndex = 0;
            if (selectedIndex < 0) selectedIndex = currentCandidateList.numElements() - 1;

            float lineHeight = ImGui::CalcTextSize("w").y;
            ImGui::BeginChild("candidates");
            for (size_t idx = 0; idx < currentCandidateList.numElements(); idx++) {
                ImVec2 cursorPos = ImGui::GetCursorPos();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                float windowWidth = ImGui::GetWindowWidth();
                ImVec2 windowPos = ImGui::GetWindowPos();

                if (selectedIndex == idx) { 
                    ImColor col = ImGui::GetStyleColorVec4(ImGuiCol_TextSelectedBg);
                    drawList->AddRectFilled(
                        ImVec2(0.0f + windowPos.x, cursorPos.y + windowPos.y - ImGui::GetScrollY()),
                        ImVec2(windowWidth + windowPos.x, cursorPos.y + lineHeight + windowPos.y - ImGui::GetScrollY()),
                        col
                    );

                    if (ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_RETURN] == 0.0f || ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_KP_ENTER] == 0.0f) {
                        ImGui::CloseCurrentPopup();
                        activate = true;
                        activateIndex = idx;
                    }

                    pushBoldFont();
                }

                drawCandidate(idx);

                if (selectedIndex == idx) {
                    ImGui::PopFont();
                    if (setScroll)
                        ImGui::SetScrollHereY();
                }
            }


            ImGui::EndChild();
            ImGui::EndPopup();
        }

        if (activate) {
            candidateSelected(activateIndex);
            currentCandidateList.clear();
            currentSearchText.clear();
        }
        ImGui::PopStyleVar(2);
        fadeAlpha += ImGui::GetIO().DeltaTime * 15.0f;
        fadeAlpha = fadeAlpha > 1.0f ? 1.0f : fadeAlpha;
    }
}