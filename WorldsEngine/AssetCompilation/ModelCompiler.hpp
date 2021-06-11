#pragma once
#include "AssetCompilers.hpp"

namespace worlds {
    class ModelCompiler : public IAssetCompiler {
    public:
        ModelCompiler();
        AssetCompileOperation* compile(AssetID src) override;
        const char* getSourceExtension() override;
        const char* getCompiledExtension() override;
    };
}
