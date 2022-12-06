#pragma once
#include <AssetCompilation/AssetCompilers.hpp>
#include <Editor/Editor.hpp>

namespace worlds
{
    class ProjectAssetCompiler
    {
      public:
        ProjectAssetCompiler(GameProject& project);
        bool isCompiling();
        void startCompiling();
        void updateCompilation();
        AssetCompileOperation* currentOperation();

      private:
        std::vector<AssetFile>::iterator assetFileCompileIterator;
        bool _isCompiling = false;
        AssetCompileOperation* currentCompileOp = nullptr;
        GameProject& project;
    };
}