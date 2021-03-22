#pragma once
#include <cstdint>
#include <physfs.h>
#include <string>

namespace worlds {
    typedef uint32_t AssetID;

    class AssetDB {
    public:
        AssetDB();
        ~AssetDB();
        void load();
        void save();
        PHYSFS_File* openAssetFileRead(AssetID id);
        PHYSFS_File* openAssetFileWrite(AssetID id);

        // There's basically no need to use this ever.
        // In 99% of casees addOrGetExisting will be the better choice
        // in case of the asset already existing in the database.
        AssetID addAsset(std::string path);

        AssetID createAsset(std::string path);
        std::string getAssetPath(AssetID id);
        std::string getAssetExtension(AssetID id);
        bool hasId(std::string path);
        bool hasId(AssetID id);
        AssetID getExistingID(std::string path);
        AssetID addOrGetExisting(std::string path);
        void rename(AssetID id, std::string newPath);
    private:
        class ADBStorage;
        AssetID currId;
        ADBStorage* storage;
        friend class AssetDBExplorer;
    };

    extern AssetDB g_assetDB;
}
