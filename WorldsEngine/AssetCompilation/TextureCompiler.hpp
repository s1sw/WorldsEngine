#pragma once
#include "AssetCompilers.hpp"
#include <nlohmann/json_fwd.hpp>

namespace worlds {
    class TextureCompiler : public IAssetCompiler {
    public:
        TextureCompiler();
        AssetCompileOperation* compile(AssetID src) override;
        const char* getSourceExtension() override;
        const char* getCompiledExtension() override;
    private:
        void compileInternal(nlohmann::json j, std::string inputPath, std::string outputPath, AssetCompileOperation* compileOp);
    };
}
