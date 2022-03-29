#include "EditorWindows.hpp"
#include <ImGui/imgui.h>
#include <Core/Log.hpp>
#include <filesystem>
#include <AssetCompilation/AssetCompilerUtil.hpp>
#include <AssetCompilation/AssetCompilers.hpp>

namespace worlds {
    static std::vector<AssetFile>::iterator assetFileCompileIterator;
    static bool isCompiling = false;
    static AssetCompileOperation* currentCompileOp = nullptr;

    void AssetCompilationManager::draw(entt::registry&) {
        ProjectAssets& assets = editor->currentProject().assets();
        if (ImGui::Begin("Asset Compilation Manager", &active)) {
            if (!isCompiling) {
                if (ImGui::Button("Discover Assets")) {
                    assets.enumerateAssets();
                }

                if (ImGui::Button("Check for changes")) {
                    assets.checkForAssetChanges();
                }

                if (ImGui::Button("Compile them!")) {
                    assetFileCompileIterator = assets.assetFiles.begin();
                    isCompiling = true;
                }
            }

            if (isCompiling) {
                if (currentCompileOp == nullptr) {
                    while (assetFileCompileIterator != assets.assetFiles.end() && (!assetFileCompileIterator->needsCompile || !assetFileCompileIterator->dependenciesExist)) {
                        assetFileCompileIterator++;
                    }
                    
                    if (assetFileCompileIterator != assets.assetFiles.end())
                        currentCompileOp = AssetCompilers::buildAsset(editor->currentProject().root(), assetFileCompileIterator->sourceAssetId);
                    else
                        isCompiling = false;
                }

                if (currentCompileOp) {
                    ImGui::Text("Compiling %s", assetFileCompileIterator->path.c_str());
                    ImGui::ProgressBar(currentCompileOp->progress);

                    if (currentCompileOp->complete) {
                        if (currentCompileOp->result != CompilationResult::Success)
                            logWarn("Failed to build %s", AssetDB::idToPath(assetFileCompileIterator->sourceAssetId).c_str());
                        else
                            assetFileCompileIterator->needsCompile = false;

                        delete currentCompileOp;
                        assetFileCompileIterator++;
                        currentCompileOp = nullptr;
                        if (assetFileCompileIterator >= assets.assetFiles.end()) {
                            isCompiling = false;
                        }
                    }
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