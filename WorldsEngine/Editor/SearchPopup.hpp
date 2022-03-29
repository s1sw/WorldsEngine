#pragma once
#include <SDL_scancode.h>
#include <Editor/GuiUtil.hpp>
#include <ImGui/imgui.h>
#include <slib/String.hpp>
#include <slib/List.hpp>

namespace worlds {
    template <typename T>
    class SearchPopup {
    public:
        virtual ~SearchPopup() {}
    protected:
        void drawPopup(const char* title) {
            ImVec2 size(500.0f, ImGui::GetTextLineHeightWithSpacing() * (3 + std::min(currentCandidateList.numElements(), (size_t)10)) + 5.0f);
            ImVec2 pos(ImGui::GetMainViewport()->Size.x * 0.5f, 200.0f);
            pos = ImVec2(pos.x - size.x * 0.5f, pos.y);
            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize(size);

            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 5.0f);
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
                bool activate = false;
                size_t activateIndex;
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

                        if (ImGui::GetIO().KeysDownDuration[SDL_SCANCODE_RETURN] == 0.0f) {
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

                if (activate) {
                    currentCandidateList.clear();
                    currentSearchText.clear();
                    candidateSelected(activateIndex);
                }

                ImGui::EndChild();
            }
            ImGui::PopStyleVar();
        }

        virtual void candidateSelected(size_t index) = 0;
        virtual void drawCandidate(size_t index) = 0;
        virtual void updateCandidates() = 0;
        size_t selectedIndex = 0;
        slib::String currentSearchText;
        slib::List<T> currentCandidateList;
    };
}