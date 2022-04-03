#include "EditorActionSearchPopup.hpp"
#include <Editor/EditorActions.hpp>
#include <Editor/Editor.hpp>
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>
#include <SDL_scancode.h>
#include <cstddef>

namespace worlds {
    EditorActionSearchPopup::EditorActionSearchPopup(Editor* ed, entt::registry& reg) 
        : ed(ed)
        , reg(reg) {}

    void EditorActionSearchPopup::show() {
        ImGui::OpenPopup("Actions");
        fadeAlpha = 0.0f;
    }

    void EditorActionSearchPopup::draw() {
        drawPopup("Actions");
    }
    
    void EditorActionSearchPopup::candidateSelected(size_t index) {
        EditorActions::getActionByHash(currentCandidateList[index]).function(ed, reg);
    }

    void EditorActionSearchPopup::drawCandidate(size_t index) {
        ImGui::Text("%s", EditorActions::getActionByHash(currentCandidateList[index]).friendlyString.cStr());
    }

    void EditorActionSearchPopup::updateCandidates() {
        currentCandidateList = EditorActions::searchForActions(currentSearchText);
    }
}