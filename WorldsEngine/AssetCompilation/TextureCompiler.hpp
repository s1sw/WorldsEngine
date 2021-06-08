#pragma once
#include "Core/AssetDB.hpp"

namespace worlds {
    class TextureCompiler {
    public:
        AssetID compile(AssetID src);
        const char* getSourceExtension();
        const char* getCompiledExtension();
    private:
    };
}
