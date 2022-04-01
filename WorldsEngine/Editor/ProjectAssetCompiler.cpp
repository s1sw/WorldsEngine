#include "ProjectAssetCompiler.hpp"
#include "AssetCompilation/AssetCompilers.hpp"
#include "Core/Log.hpp"
#include "Editor/Editor.hpp"

namespace worlds {
    ProjectAssetCompiler::ProjectAssetCompiler(GameProject& project)
        : project(project) {}

    bool ProjectAssetCompiler::isCompiling() {
        return _isCompiling;
    }

    void ProjectAssetCompiler::startCompiling() {
        if (_isCompiling) return;

        _isCompiling = true;
        assetFileCompileIterator = project.assets().assetFiles.begin();
    }

    void ProjectAssetCompiler::updateCompilation() {
        const ProjectAssets& assets = project.assets();
        if (_isCompiling) {
            if (currentCompileOp == nullptr) {
                while (assetFileCompileIterator != assets.assetFiles.end() && (!assetFileCompileIterator->needsCompile || !assetFileCompileIterator->dependenciesExist)) {
                    assetFileCompileIterator++;
                }
                
                if (assetFileCompileIterator != assets.assetFiles.end())
                    currentCompileOp = AssetCompilers::buildAsset(project.root(), assetFileCompileIterator->sourceAssetId);
                else
                    _isCompiling = false;
            }

            if (currentCompileOp) {
                if (currentCompileOp->complete) {
                    if (currentCompileOp->result != CompilationResult::Success)
                        logWarn("Failed to build %s", AssetDB::idToPath(assetFileCompileIterator->sourceAssetId).c_str());
                    else
                        assetFileCompileIterator->needsCompile = false;

                    delete currentCompileOp;
                    assetFileCompileIterator++;
                    currentCompileOp = nullptr;
                    if (assetFileCompileIterator >= assets.assetFiles.end()) {
                        _isCompiling = false;
                    }
                }
            }
        }
    }

    AssetCompileOperation* ProjectAssetCompiler::currentOperation() {
        return currentCompileOp;
    }
}