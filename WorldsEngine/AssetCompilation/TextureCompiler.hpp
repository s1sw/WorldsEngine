#pragma once
#include "AssetCompilers.hpp"
#include <nlohmann/json_fwd.hpp>

namespace worlds {
    class TextureCompiler : public IAssetCompiler {
    public:
        TextureCompiler();
        AssetCompileOperation* compile(std::string_view projectRoot, AssetID src) override;
        const char* getSourceExtension() override;
        const char* getCompiledExtension() override;
    private:
        struct TexCompileThreadInfo;
        void compileInternal(TexCompileThreadInfo*);
    };
}
