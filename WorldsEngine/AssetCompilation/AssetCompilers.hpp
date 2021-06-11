#pragma once
#include "../Core/AssetDB.hpp"

namespace worlds {
    class IAssetCompiler {
    public:
        virtual AssetID compile(AssetID src) = 0;
        virtual const char* getSourceExtension() = 0;
        virtual const char* getCompiledExtension() = 0;
        virtual ~IAssetCompiler() {}
    };

    class AssetCompilers {
    public:
        static void initialise();
        static void registerCompiler(IAssetCompiler* compiler);
        static AssetID buildAsset(AssetID asset);
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
