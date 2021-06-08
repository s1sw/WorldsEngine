#include "AssetDB.hpp"
#include <filesystem>
#include <iostream>
#include "Core/Log.hpp"
#include "Fatal.hpp"
#include <mutex>
#include <string.h>
#include <robin_hood.h>

namespace worlds {
    class ADBStorage {
    public:
        std::mutex mutex;
        robin_hood::unordered_map<AssetID, std::string> paths;
        robin_hood::unordered_map<AssetID, std::string> extensions;
    };

    class FnvHash {
        static const unsigned int FNV_PRIME = 16777619u;
        static const unsigned int OFFSET_BASIS = 2166136261u;

        template <unsigned int N>
        static constexpr unsigned int fnvHashConst(const char (&str)[N], unsigned int I = N) {
            return I == 1 ? (OFFSET_BASIS ^ str[0]) * FNV_PRIME : (fnvHashConst(str, I - 1) ^ str[I - 1]) * FNV_PRIME;
        }

        static unsigned int fnvHash(const char* str) {
            const size_t length = strlen(str) + 1;
            unsigned int hash = OFFSET_BASIS;
            for (size_t i = 0; i < length; ++i)
            {
                hash ^= *str++;
                hash *= FNV_PRIME;
            }
            return hash;
        }

        struct Wrapper {
            Wrapper(const char* str) : str (str) { }
            const char* str;
        };

        unsigned int hash_value;
    public:
        // calulate in run-time
        FnvHash(Wrapper wrapper) : hash_value(fnvHash(wrapper.str)) { }
        // calulate in compile-time
        template <unsigned int N>
        constexpr FnvHash(const char (&str)[N]) : hash_value(fnvHashConst(str)) { }
        // output result
        constexpr operator unsigned int() const { return this->hash_value; }
    };

    const char* ASSET_DB_PATH = "assetDB.wdb";
    const char ASSET_DB_MAGIC[] = { 'W','A','D','B' };
    const uint8_t ASSET_DB_VER = 1;
    ADBStorage storage;

    void AssetDB::load() {
        if (!PHYSFS_exists(ASSET_DB_PATH)) {
            logWarn("Asset DB doesn't exist! Oh well.");
            return;
        }
        std::lock_guard<std::mutex> lg{storage.mutex};
        PHYSFS_File* dbFile = PHYSFS_openRead(ASSET_DB_PATH);

        char magicBuf[5];

        // Add a null byte to make it a valid null-terminated string just in case
        magicBuf[4] = 0;

        PHYSFS_readBytes(dbFile, magicBuf, 4);

        if (memcmp(magicBuf, ASSET_DB_MAGIC, 4) != 0) {
            fatalErr("AssetDB magic was invalid. Data is likely corrupted.");
        }

        uint8_t ver;
        PHYSFS_readBytes(dbFile, &ver, sizeof(ver));

        uint32_t idCount;
        PHYSFS_readULE32(dbFile, &idCount);

        storage.paths.reserve(idCount);

        uint32_t maxId = 0;

        for (uint32_t i = 0; i < idCount; i++) {
            std::string path;

            uint16_t pathLen;
            PHYSFS_readULE16(dbFile, &pathLen);
            path.resize(pathLen);

            PHYSFS_readBytes(dbFile, path.data(), pathLen);

            uint32_t id;
            PHYSFS_readULE32(dbFile, &id);
            storage.paths.insert({ id, path });

            auto ext = std::filesystem::path(path).extension().string();
            storage.extensions.insert({ id, ext });

            maxId = std::max(id, maxId);
        }

        PHYSFS_close(dbFile);
    }

    void AssetDB::save() {
        std::lock_guard<std::mutex> lg{storage.mutex};
        PHYSFS_File* dbFile = PHYSFS_openWrite(ASSET_DB_PATH);
        PHYSFS_writeBytes(dbFile, ASSET_DB_MAGIC, sizeof(ASSET_DB_MAGIC));
        PHYSFS_writeBytes(dbFile, &ASSET_DB_VER, sizeof(ASSET_DB_VER));
        PHYSFS_writeULE32(dbFile, (uint32_t)storage.paths.size());

        for (auto& p : storage.paths) {
            PHYSFS_writeULE16(dbFile, (uint16_t)p.second.size());
            PHYSFS_writeBytes(dbFile, p.second.data(), p.second.size());
            PHYSFS_writeULE32(dbFile, p.first);
        }

        PHYSFS_close(dbFile);
    }

    PHYSFS_File* AssetDB::openAssetFileRead(AssetID id) {
        std::lock_guard<std::mutex> lg{storage.mutex};
        return PHYSFS_openRead(storage.paths.at(id).c_str());
    }

    PHYSFS_File* AssetDB::openAssetFileWrite(AssetID id) {
        std::lock_guard<std::mutex> lg{storage.mutex};
        return PHYSFS_openWrite(storage.paths.at(id).c_str());
    }

    AssetID AssetDB::addAsset(std::string_view path) {
        std::lock_guard<std::mutex> lg{storage.mutex};
        if (PHYSFS_exists(path.data()) == 0) {
            logWarn("Adding missing asset: %s", path.data());
        }

        AssetID id = FnvHash(path.data());


        // Figure out the file extension
        auto ext = std::filesystem::path(path).extension().string();
        logMsg("Added asset %s with extension %s", path.data(), ext);
        storage.extensions.insert({ id, ext });
        storage.paths.insert({ id, path });

        return id;
    }

    AssetID AssetDB::createAsset(std::string_view path) {
        std::lock_guard<std::mutex> lg{storage.mutex};
        PHYSFS_close(PHYSFS_openWrite(path.data()));
        return addAsset(path);
    }

    std::string AssetDB::idToPath(AssetID id) {
        std::lock_guard<std::mutex> lg{storage.mutex};
        return storage.paths.at(id);
    }

    std::string AssetDB::getAssetExtension(AssetID id) {
        std::lock_guard<std::mutex> lg{storage.mutex};
        return storage.extensions.at(id);
    }

    AssetID AssetDB::pathToId(std::string_view path) {
        uint32_t hash = FnvHash(path.data());

        if (!storage.paths.contains(hash)) {
            return addAsset(path);
        }

        return hash;
    }

    bool AssetDB::exists(AssetID id) {
        return storage.paths.contains(id) && PHYSFS_exists(storage.paths.at(id).c_str());
    }
}
