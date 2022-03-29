#include "EditorAssetSearchPopup.hpp"
#include "Editor/GuiUtil.hpp"
#include <Editor/Editor.hpp>
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>
#include <Core/AssetDB.hpp>

namespace worlds {
    EditorAssetSearchPopup::EditorAssetSearchPopup(Editor* ed) : editor(ed) {}

    void EditorAssetSearchPopup::show() {
        ImGui::OpenPopup("Asset Search");
    }

    void EditorAssetSearchPopup::draw() {
        drawPopup("Asset Search");
    }

    void EditorAssetSearchPopup::candidateSelected(size_t index) {
        editor->openAsset(currentCandidateList[index]);
    }

    void EditorAssetSearchPopup::drawCandidate(size_t index) {
        int skipChars = sizeof("SourceData/") - 1;
        ImGui::Text("%s", AssetDB::idToPath(currentCandidateList[index]).c_str() + skipChars);
    }

    void EditorAssetSearchPopup::updateCandidates() {
        currentCandidateList = editor->currentProject().assets().searchForAssets(currentSearchText);
    }
}