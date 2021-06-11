#include "AssetCompilers.hpp"
#include <robin_hood.h>
#include <slib/List.hpp>
#include "../Util/Fnv.hpp"

#include "TextureCompiler.hpp"
#include "ModelCompiler.hpp"
// Compilers
namespace worlds {
    namespace asset_compilers {
        TextureCompiler tc;
        ModelCompiler mc;
    }
}

namespace worlds {
    slib::List<IAssetCompiler*> compilers;
    robin_hood::unordered_flat_map<uint32_t, IAssetCompiler*> extensionCompilers;
    AssetCompilers::StaticLink* AssetCompilers::staticLink;

    void AssetCompilers::initialise() {
        StaticLink* current = staticLink;
        while (current) {
            compilers.add(current->compiler);
            uint32_t extensionHash = FnvHash(current->compiler->getSourceExtension());
            extensionCompilers.insert({
                extensionHash,
                current->compiler
            });

            StaticLink* tmp = current;
            current = current->next;
            delete tmp;
        }
    }

    void AssetCompilers::registerCompiler(IAssetCompiler* compiler) {
        StaticLink* sl = new StaticLink {
            .compiler = compiler,
            .next = staticLink
        };
        staticLink = sl;
    }

    AssetCompileOperation* AssetCompilers::buildAsset(AssetID asset) {
        return getCompilerFor(asset)->compile(asset);
    }

    IAssetCompiler* AssetCompilers::getCompilerFor(AssetID asset) {
        std::string ext = AssetDB::getAssetExtension(asset);

        return getCompilerFor(ext);
    }

    IAssetCompiler* AssetCompilers::getCompilerFor(std::string_view extension) {
        uint32_t hash = FnvHash(extension);
        auto it = extensionCompilers.find(hash);

        if (it == extensionCompilers.end()) {
            return nullptr;
        }

        return it->second;
    }
}
