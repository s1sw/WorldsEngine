#include "RawTextureLoader.hpp"
#include <vector>

#include <Core/AssetDB.hpp>
#include <Core/Log.hpp>
#include <Libs/miniz.h>
#include <stb_image.h>
#include <tracy/Tracy.hpp>
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>

namespace worlds
{
    bool RawTextureLoader::loadStbTexture(void *fileData, size_t fileLen, AssetID id, RawTextureData &texData)
    {
        ZoneScoped;
        int x, y, channelsInFile;
        bool hdr = false;
        stbi_uc *dat;

        if (AssetDB::getAssetExtension(id) == ".hdr")
        {
            float *fpDat;
            fpDat = stbi_loadf_from_memory((stbi_uc *)fileData, (int)fileLen, &x, &y, &channelsInFile, 4);
            dat = (stbi_uc *)fpDat;
            hdr = true;
        }
        else
        {
            dat = stbi_load_from_memory((stbi_uc *)fileData, (int)fileLen, &x, &y, &channelsInFile, 4);
        }

        if (dat == nullptr)
        {
            logErr("STB Image error: %s", stbi_failure_reason());
            return false;
        }

        texData.data = dat;
        texData.width = x;
        texData.height = y;
        texData.format = hdr ? RawTextureFormat::RGBA32F : RawTextureFormat::RGBA8;
        texData.totalDataSize = hdr ? x * y * 4 * sizeof(float) : x * y * 4;

        return true;
    }

    bool RawTextureLoader::loadExrTexture(void *fileData, size_t fileLen, AssetID id, RawTextureData &texData)
    {
        ZoneScoped;

        float *data;
        int width, height;
        const char *err;
        int result = LoadEXRFromMemory(&data, &width, &height, (uint8_t *)fileData, fileLen, &err);

        if (result != TINYEXR_SUCCESS)
        {
            logErr("Error loading %s: %s", AssetDB::idToPath(id).c_str(), err);
            return false;
        }

        texData.data = (uint8_t *)data;
        texData.width = width;
        texData.height = height;
        texData.format = RawTextureFormat::RGBA32F;
        texData.totalDataSize = width * height * 4 * sizeof(float);

        return true;
    }

    bool RawTextureLoader::loadRawTexture(AssetID id, RawTextureData &texData)
    {
        ZoneScoped;

        if (!AssetDB::exists(id))
        {
            return false;
        }

        PHYSFS_File *file = AssetDB::openAssetFileRead(id);
        if (!file)
        {
            std::string path = AssetDB::idToPath(id);
            auto errCode = PHYSFS_getLastErrorCode();
            logErr(WELogCategoryEngine, "Failed to load texture %s: %s", path.c_str(), PHYSFS_getErrorByCode(errCode));
            return false;
        }

        size_t fileLen = PHYSFS_fileLength(file);
        std::vector<uint8_t> fileVec(fileLen);

        PHYSFS_readBytes(file, fileVec.data(), fileLen);
        PHYSFS_close(file);

        std::string ext = AssetDB::getAssetExtension(id);

        if (AssetDB::getAssetExtension(id) == ".exr")
        {
            return loadExrTexture(fileVec.data(), fileLen, id, texData);
        }

        return loadStbTexture(fileVec.data(), fileLen, id, texData);
    }
}
