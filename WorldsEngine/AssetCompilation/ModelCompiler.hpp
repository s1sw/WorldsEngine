#pragma once
#include "AssetCompilers.hpp"

namespace worlds
{
    class ModelCompiler : public IAssetCompiler
    {
      public:
        ModelCompiler();
        AssetCompileOperation *compile(std::string_view projectRoot, AssetID src) override;
        void getFileDependencies(AssetID src, std::vector<std::string> &out) override;
        const char *getSourceExtension() override;
        const char *getCompiledExtension() override;
    };
}
