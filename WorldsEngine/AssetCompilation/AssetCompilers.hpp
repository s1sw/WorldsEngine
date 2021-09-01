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
        virtual AssetCompileOperation* compile(std::string_view projectRoot, AssetID src) = 0;
        virtual const char* getSourceExtension() = 0;
        virtual const char* getCompiledExtension() = 0;
        virtual ~IAssetCompiler() {}
    };

    class AssetCompilers {
    public:
        static void initialise();
        static void registerCompiler(IAssetCompiler* compiler);
        static AssetCompileOperation* buildAsset(std::string_view projectRoot, AssetID asset);
        static IAssetCompiler* getCompilerFor(AssetID asset);
        static IAssetCompiler* getCompilerFor(std::string_view extension);
        static size_t registeredCompilerCount();
        static IAssetCompiler** registeredCompilers();
    private:
        struct StaticLink {
            IAssetCompiler* compiler;
            StaticLink* next;
        };

        static StaticLink* staticLink;
    };
}
