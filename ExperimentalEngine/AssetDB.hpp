#pragma once
#include <cstdint>
#include <unordered_map>
#include <physfs.h>
#include <cassert>
#include <string>

namespace worlds {
    typedef uint32_t AssetID;

    enum class AssetMetaValueType {
        Int,
        Int64,
        String,
        Float
    };

    struct AssetMetaValue {
        AssetMetaValueType type;
        union {
            int32_t intVal;
            int64_t int64Val;
            std::string stringVal;
            float floatVal;
        };

        operator int32_t() {
            assert(type == AssetMetaValueType::Int);
            return intVal;
        }

        operator int64_t() {
            assert(type == AssetMetaValueType::Int64);
            return int64Val;
        }

        operator std::string() {
            assert(type == AssetMetaValueType::String);
            return stringVal;
        }

        operator float() {
            assert(type == AssetMetaValueType::Float);
            return floatVal;
        }

        ~AssetMetaValue() {

        }
    };

    struct AssetMeta {
        std::unordered_map<std::string, AssetMetaValue> metaValues;
    };

    class AssetDB {
    public:
        AssetDB();
        void load();
        void save();
        PHYSFS_File* openAssetFileRead(AssetID id);
        PHYSFS_File* openAssetFileWrite(AssetID id);

        // There's basically no need to use this ever.
        // In 99% of casees addOrGetExisting will be the better choice
        // in case of the asset already existing in the database.
        AssetID addAsset(std::string path);

        AssetID createAsset(std::string path);
        std::string getAssetPath(AssetID id) { return paths[id]; }
        std::string getAssetExtension(AssetID id) { return extensions[id]; }
        bool hasId(std::string path) { return ids.find(path) != ids.end(); }
        AssetID getExistingID(std::string path) { return ids.at(path); }
        AssetID addOrGetExisting(std::string path) { return hasId(path) ? getExistingID(path) : addAsset(path); }
    private:
        AssetID currId;
        std::unordered_map<AssetID, std::string> paths;
        std::unordered_map<std::string, AssetID> ids;
        std::unordered_map<AssetID, std::string> extensions;
    };

    extern AssetDB g_assetDB;
}
