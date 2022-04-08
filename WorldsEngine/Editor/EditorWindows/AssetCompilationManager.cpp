#include "Editor/Editor.hpp"
#include "EditorWindows.hpp"
#include <ImGui/imgui.h>
#include <Core/Log.hpp>
#include <filesystem>
#include <AssetCompilation/AssetCompilerUtil.hpp>
#include <AssetCompilation/AssetCompilers.hpp>
#include <Editor/ProjectAssetCompiler.hpp>

namespace worlds {
    void AssetCompilationManager::draw(entt::registry&) {
        ProjectAssets& assets = editor->currentProject().assets();
        ProjectAssetCompiler& assetCompiler = editor->currentProject().assetCompiler();

        if (ImGui::Begin("Asset Compilation Manager", &active)) {
            if (!assetCompiler.isCompiling()) {
                if (ImGui::Button("Discover Assets")) {
                    assets.enumerateAssets();
                }

                if (ImGui::Button("Check for changes")) {
                    assets.checkForAssetChanges();
                }

                if (ImGui::Button("Compile them!")) {
                    assetCompiler.startCompiling();
                }
            } else  {
                if (assetCompiler.currentOperation()) {
                    AssetCompileOperation* currentOp = assetCompiler.currentOperation();
                    ImGui::Text("Compiling %s", AssetDB::idToPath(currentOp->outputId).c_str());
                    ImGui::ProgressBar(currentOp->progress);
                }
            }

            for (auto& file : assets.assetFiles) {
                if (!file.isCompiled) continue;

                ImGui::Text("%s -> %s", file.path.c_str(), file.compiledPath.c_str());

                if (file.needsCompile) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "(needs compile)");
                }

                if (!file.dependenciesExist) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "(dependencies missing)");
                }
            }
        }
        ImGui::End();
    }
}