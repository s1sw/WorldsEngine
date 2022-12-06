#include "Core/AssetDB.hpp"
#include "Export.hpp"
#include <cstring>

using namespace worlds;

extern "C"
{
    EXPORT void assetDB_idToPath(uint32_t id, uint32_t* length, char* outBuffer)
    {
        std::string path = AssetDB::idToPath(id);

        *length = path.size();

        if (outBuffer)
        {
            outBuffer[path.size()] = 0;
            strncpy(outBuffer, path.c_str(), path.size());
        }
    }

    EXPORT uint32_t assetDB_pathToId(char* path)
    {
        return AssetDB::pathToId(path);
    }

    EXPORT char assetDB_exists(uint32_t id)
    {
        return AssetDB::exists(id);
    }

    EXPORT PHYSFS_File* assetDB_openAssetRead(uint32_t id)
    {
        return AssetDB::openAssetFileRead(id);
    }

    EXPORT PHYSFS_File* assetDB_openAssetWrite(uint32_t id)
    {
        return AssetDB::openAssetFileWrite(id);
    }

    EXPORT uint32_t assetDB_createAsset(const char* path)
    {
        return AssetDB::createAsset(path);
    }
}
