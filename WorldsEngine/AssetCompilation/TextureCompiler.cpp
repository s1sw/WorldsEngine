#include "TextureCompiler.hpp"

namespace worlds {
    AssetID TextureCompiler::compile(AssetID src) {
    }

    const char* TextureCompiler::getSourceExtension() {
        return "wtexj";
    }

    const char* TextureCompiler::getCompiledExtension() {
        return "wtex";
    }
}
