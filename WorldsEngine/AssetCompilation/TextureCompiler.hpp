#pragma once
#include "AssetCompilers.hpp"

namespace worlds {
    class TextureCompiler : public IAssetCompiler {
    public:
        TextureCompiler();
        AssetID compile(AssetID src);
        const char* getSourceExtension();
        const char* getCompiledExtension();
    private:
    };
}
