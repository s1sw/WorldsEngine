#include "EditorWindows.hpp"
#include "../../AssetCompilation/AssetCompilers.hpp"
#include "../AssetEditors.hpp"

namespace worlds {
    void AssetEditor::draw(entt::registry& reg) {
        if (ImGui::Begin("Asset Editor", &active)) {
            if (editor->currentSelectedAsset == INVALID_ASSET) {
                ImGui::Text("No asset currently selected.");
            } else {
                IAssetEditor* assetEditor = AssetEditors::getEditorFor(editor->currentSelectedAsset);
                if (editor->currentSelectedAsset != lastId) {
                    if (lastId != INVALID_ASSET) {
                        IAssetEditor* lastEditor = AssetEditors::getEditorFor(lastId);
                        lastEditor->save();
                    }
                    assetEditor->open(editor->currentSelectedAsset);
                }

                std::string path = AssetDB::idToPath(editor->currentSelectedAsset);
                ImGui::Text("Path: %s", path.c_str());

                if (ImGui::Button("Save")) {
                    assetEditor->save();
                }

                ImGui::SameLine();

                if (ImGui::Button("Compile")) {
                    AssetCompilers::buildAsset(editor->currentSelectedAsset);
                }
            }
            lastId = editor->currentSelectedAsset;
        }
        ImGui::End();
    }
}
