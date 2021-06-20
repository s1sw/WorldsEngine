#include "EditorWindows.hpp"
#include "../../AssetCompilation/AssetCompilers.hpp"
#include "../AssetEditors.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../ImGui/imgui_internal.h"

namespace worlds {
    void AssetEditor::draw(entt::registry& reg) {
        static AssetCompileOperation* currCompileOp = nullptr;
        if (ImGui::Begin("Asset Editor", &active)) {
            if (editor->currentSelectedAsset == INVALID_ASSET) {
                ImGui::Text("No asset currently selected.");
            } else {
                IAssetEditor* assetEditor = AssetEditors::getEditorFor(editor->currentSelectedAsset);
                if (editor->currentSelectedAsset != lastId && assetEditor) {
                    if (lastId != INVALID_ASSET) {
                        IAssetEditor* lastEditor = AssetEditors::getEditorFor(lastId);
                        if (lastEditor)
                            lastEditor->save();
                    }
                    assetEditor->open(editor->currentSelectedAsset);
                }

                std::string path = AssetDB::idToPath(editor->currentSelectedAsset);
                ImGui::Text("Path: %s", path.c_str());

                ImGui::SameLine();

                if (ImGui::Button("Close")) {
                    editor->currentSelectedAsset = INVALID_ASSET;
                }

                if (assetEditor)
                    assetEditor->drawEditor();
                else
                    ImGui::TextColored(ImColor(1.0f, 0.0f, 0.0f), "OOF");

                if (ImGui::Button("Save")) {
                    assetEditor->save();
                }

                if (currCompileOp) {
                    ImGui::ProgressBar(currCompileOp->progress);

                    if (currCompileOp->complete) {
                        delete currCompileOp;
                        currCompileOp = nullptr;
                    }
                } else {
                    if (ImGui::Button("Compile")) {
                        currCompileOp = AssetCompilers::buildAsset(editor->currentSelectedAsset);
                    }
                }
            }
            lastId = editor->currentSelectedAsset;
        }
        ImGui::End();
    }
}
