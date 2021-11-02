#pragma once
#include <cstdint>
#include <physfs.h>
#include <string_view>

namespace worlds {
    typedef uint32_t AssetID;
    static constexpr AssetID INVALID_ASSET = ~0u;

    class AssetDB {
    public:
        static void load();
        static void save();
        static PHYSFS_File* openAssetFileRead(AssetID id);
        static PHYSFS_File* openAssetFileWrite(AssetID id);

        static AssetID createAsset(std::string_view path);
        static std::string getAssetExtension(AssetID id);
        static std::string idToPath(AssetID id);
        static AssetID pathToId(std::string_view path);
        static bool exists(AssetID id);
    private:
        static AssetID addAsset(std::string_view path);
        friend class AssetDBExplorer;
    };
}
