#pragma once
#include "../Core/AssetDB.hpp"

namespace worlds {
    struct AssetCompileOperation {
        AssetID outputId;
        bool complete = false;
        float progress = 0.0f;
    };

    class IAssetCompiler {
    public:
        virtual AssetCompileOperation* compile(AssetID src) = 0;
        virtual const char* getSourceExtension() = 0;
        virtual const char* getCompiledExtension() = 0;
        virtual ~IAssetCompiler() {}
    };

    class AssetCompilers {
    public:
        static void initialise();
        static void registerCompiler(IAssetCompiler* compiler);
        static AssetCompileOperation* buildAsset(AssetID asset);
        static IAssetCompiler* getCompilerFor(AssetID asset);
        static IAssetCompiler* getCompilerFor(std::string_view extension);
    private:
        struct StaticLink {
            IAssetCompiler* compiler;
            StaticLink* next;
        };

        static StaticLink* staticLink;
    };
}
