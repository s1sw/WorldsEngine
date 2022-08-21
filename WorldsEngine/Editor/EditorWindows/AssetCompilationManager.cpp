#include "EditorWindows.hpp"
#include <AssetCompilation/AssetCompilerUtil.hpp>
#include <AssetCompilation/AssetCompilers.hpp>
#include <Core/Log.hpp>
#include <Editor/Editor.hpp>
#include <Editor/ProjectAssetCompiler.hpp>
#include <ImGui/imgui.h>

namespace worlds
{
    void AssetCompilationManager::draw(entt::registry&)
    {
        ProjectAssets& assets = editor->currentProject().assets();
        ProjectAssetCompiler& assetCompiler = editor->currentProject().assetCompiler();

        if (ImGui::Begin("Asset Compilation Manager", &active))
        {
            if (!assetCompiler.isCompiling())
            {
                if (ImGui::Button("Discover Assets"))
                {
                    assets.enumerateAssets();
                }

                if (ImGui::Button("Check for changes"))
                {
                    assets.checkForAssetChanges();
                }

                if (ImGui::Button("Compile them!"))
                {
                    assetCompiler.startCompiling();
                }
            }
            else
            {
                if (assetCompiler.currentOperation())
                {
                    AssetCompileOperation* currentOp = assetCompiler.currentOperation();
                    ImGui::Text("Compiling %s", AssetDB::idToPath(currentOp->outputId).c_str());
                    ImGui::ProgressBar(currentOp->progress);
                }
            }

            ImGui::Text("%zu assets registered", assets.assetFiles.size());

            for (auto& file : assets.assetFiles)
            {
                if (!file.isCompiled)
                    continue;

                ImGui::Text("%s -> %s", file.path.c_str(), file.compiledPath.c_str());

                if (file.needsCompile)
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "(needs compile)");
                }

                if (!file.dependenciesExist)
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "(dependencies missing)");
                }
            }
        }
        ImGui::End();
    }
}