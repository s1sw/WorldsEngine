#include "EditorWindows.hpp"
#include <ImGui/imgui.h>
#include <Core/Log.hpp>
#include <filesystem>
#include <AssetCompilation/AssetCompilerUtil.hpp>
#include <AssetCompilation/AssetCompilers.hpp>

namespace worlds {
    struct AssetFile {
        AssetID sourceAssetId;
        std::string path;
        int64_t lastModified;
        std::string compiledPath;
        bool needsCompile = false;
        bool dependenciesExist = true;
    };

    static std::vector<AssetFile> assetFiles;
    static std::vector<AssetFile>::iterator assetFileCompileIterator;
    static bool isCompiling = false;
    static AssetCompileOperation* currentCompileOp = nullptr;

    void enumerateForAssets(const char* path) {
        char** list = PHYSFS_enumerateFiles(path);

        for (char** p = list; *p != nullptr; p++) {
            char* subpath = *p;
            std::string fullPath = std::string(path) + '/' + subpath;

            PHYSFS_Stat stat;
            PHYSFS_stat(fullPath.c_str(), &stat);

            logMsg("f: %s", subpath);
            if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                enumerateForAssets(fullPath.c_str());
            } else if (stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                std::filesystem::path sp = subpath;
                if (sp.has_extension()) {
                    std::string extension = std::filesystem::path{ subpath }.extension().string();

                    if (extension == ".wmdlj" || extension == ".wtexj") {
                        assetFiles.push_back(AssetFile{
                            .sourceAssetId = AssetDB::pathToId(fullPath),
                            .path = fullPath,
                            .lastModified = stat.modtime,
                            .compiledPath = getOutputPath(fullPath).substr(5)
                        });
                    }
                }
            }
        }

        PHYSFS_freeList(list);
    }

    void checkForAssetChange(AssetFile& file) {
        PHYSFS_Stat compiledStat;
        PHYSFS_stat(file.compiledPath.c_str(), &compiledStat);

        if (compiledStat.modtime < file.lastModified)
            file.needsCompile = true;

        IAssetCompiler* compiler = AssetCompilers::getCompilerFor(file.sourceAssetId);
        std::vector<std::string> dependencies;
        compiler->getFileDependencies(file.sourceAssetId, dependencies);

        file.dependenciesExist = true;
        for (auto& dependency : dependencies) {
            if (!PHYSFS_exists(dependency.c_str())) {
                file.dependenciesExist = false;
                continue;
            }

            PHYSFS_Stat dependencyStat;
            PHYSFS_stat(dependency.c_str(), &dependencyStat);

            if (dependencyStat.modtime > compiledStat.modtime)
                file.needsCompile = true;
        }
    }

    void AssetCompilationManager::draw(entt::registry&) {
        if (ImGui::Begin("Asset Compilation Manager", &active)) {
            if (!isCompiling) {
                if (ImGui::Button("Discover Assets")) {
                    assetFiles.clear();
                    enumerateForAssets("SourceData");
                }

                if (ImGui::Button("Check for changes")) {
                    for (auto& file : assetFiles) {
                        if (!PHYSFS_exists(file.compiledPath.c_str())) {
                            file.needsCompile = true;
                            continue;
                        }

                        checkForAssetChange(file);
                    }
                }

                if (ImGui::Button("Compile them!")) {
                    assetFileCompileIterator = assetFiles.begin();
                    isCompiling = true;
                }
            }

            if (isCompiling) {
                if (currentCompileOp == nullptr) {
                    while (assetFileCompileIterator != assetFiles.end() && (!assetFileCompileIterator->needsCompile || !assetFileCompileIterator->dependenciesExist)) {
                        assetFileCompileIterator++;
                    }
                    
                    if (assetFileCompileIterator != assetFiles.end())
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
                        if (assetFileCompileIterator >= assetFiles.end()) {
                            isCompiling = false;
                        }
                    }
                }
            }

            for (auto& file : assetFiles) {
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