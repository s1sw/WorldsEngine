#define CRND_HEADER_FILE_ONLY
#include "TextureLoader.hpp"
#include "crn_decomp.h"
#include "stb_image.h"
#include "Tracy.hpp"
#include <Core/Engine.hpp>
#include <Core/Fatal.hpp>
#include <Core/Log.hpp>
#include <Core/TaskScheduler.hpp>
#include <Render/RenderInternal.hpp>
#include <WTex.hpp>
#include <algorithm>
#include <mutex>
#include <physfs.h>
#include <nlohmann/json.hpp>

using namespace R2;

namespace worlds
{
    std::mutex vkMutex;

    uint32_t getCrunchTextureSize(crnd::crn_texture_info texInfo, int mip)
    {
        const crn_uint32 width = std::max(1U, texInfo.m_width >> mip);
        const crn_uint32 height = std::max(1U, texInfo.m_height >> mip);
        const crn_uint32 blocks_x = std::max(1U, (width + 3) >> 2);
        const crn_uint32 blocks_y = std::max(1U, (height + 3) >> 2);
        const crn_uint32 row_pitch = blocks_x * crnd::crnd_get_bytes_per_dxt_block(texInfo.m_format);
        const crn_uint32 total_face_size = row_pitch * blocks_y;

        return total_face_size;
    }

    uint32_t getRowPitch(crnd::crn_texture_info texInfo, int mip)
    {
        const crn_uint32 width = std::max(1U, texInfo.m_width >> mip);
        const crn_uint32 blocks_x = std::max(1U, (width + 3) >> 2);
        const crn_uint32 row_pitch = blocks_x * crnd::crnd_get_bytes_per_dxt_block(texInfo.m_format);

        return row_pitch;
    }

    inline int getNumMips(int w, int h)
    {
        return (int)(1 + floor(log2(glm::max(w, h))));
    }

    TextureData loadCrunchTexture(void* fileData, size_t fileLen, AssetID id)
    {
        ZoneScoped;

        crnd::crn_texture_info texInfo{};

        if (!crnd::crnd_get_texture_info(fileData, (uint32_t)fileLen, &texInfo))
            return TextureData{};

        bool isSRGB = texInfo.m_userdata0;
        crnd::crnd_unpack_context context = crnd::crnd_unpack_begin(fileData, (uint32_t)fileLen);

        crn_format fundamentalFormat = crnd::crnd_get_fundamental_dxt_format(texInfo.m_format);

        VK::TextureFormat format{};

        switch (fundamentalFormat)
        {
        case crn_format::cCRNFmtDXT1:
            format = isSRGB ? VK::TextureFormat::BC1_RGBA_SRGB_BLOCK : VK::TextureFormat::BC1_RGBA_UNORM_BLOCK;
            break;
        case crn_format::cCRNFmtDXT5:
            format = isSRGB ? VK::TextureFormat::BC3_SRGB_BLOCK : VK::TextureFormat::BC3_UNORM_BLOCK;
            break;
        case crn_format::cCRNFmtDXN_XY:
            format = VK::TextureFormat::BC5_UNORM_BLOCK;
            break;
        default:
            format = VK::TextureFormat::UNDEFINED;
            break;
        }

        uint32_t x = texInfo.m_width;
        uint32_t y = texInfo.m_height;

        size_t totalDataSize = 0;
        for (uint32_t i = 0; i < texInfo.m_levels; i++)
            totalDataSize += getCrunchTextureSize(texInfo, i);

        char* data = (char*)std::malloc(totalDataSize);
        size_t currOffset = 0;
        for (uint32_t i = 0; i < texInfo.m_levels; i++)
        {
            char* dataOffs = &data[currOffset];
            uint32_t dataSize = getCrunchTextureSize(texInfo, i);
            currOffset += dataSize;

            if (!crnd::crnd_unpack_level(context, (void**)&dataOffs, dataSize, getRowPitch(texInfo, i), i))
                fatalErr("Failed to unpack texture");
        }

        uint32_t numMips = texInfo.m_levels;

        crnd::crnd_unpack_end(context);

        TextureData td{};

        td.data = (uint8_t*)data;
        td.numMips = numMips;
        td.width = x;
        td.height = y;
        td.format = format;
        td.name = AssetDB::idToPath(id);
        td.totalDataSize = (uint32_t)totalDataSize;

        return td;
    }

    TextureData loadStbTexture(void* fileData, size_t fileLen, AssetID id)
    {
        ZoneScoped;
        int x, y, channelsInFile;
        bool hdr = false;
        bool forceLinear = false;
        stbi_uc* dat;
        std::string path = AssetDB::idToPath(id);
        if (path.find("forcelin") != std::string::npos)
        {
            forceLinear = true;
        }
        if (AssetDB::getAssetExtension(id) == ".hdr")
        {
            float* fpDat;
            fpDat = stbi_loadf_from_memory((stbi_uc*)fileData, (int)fileLen, &x, &y, &channelsInFile, 4);
            dat = (stbi_uc*)fpDat;
            hdr = true;
        }
        else
        {
            dat = stbi_load_from_memory((stbi_uc*)fileData, (int)fileLen, &x, &y, &channelsInFile, 4);
        }

        if (dat == nullptr)
        {
            logErr("STB Image error");
        }

        TextureData td{};

        td.data = dat;
        td.numMips = 1;
        td.width = (uint32_t)x;
        td.height = (uint32_t)y;

        if (hdr)
            td.format = VK::TextureFormat::R32G32B32A32_SFLOAT;
        else if (!forceLinear)
            td.format = VK::TextureFormat::R8G8B8A8_SRGB;
        else
            td.format = VK::TextureFormat::R8G8B8A8_UNORM;

        td.name = AssetDB::idToPath(id);
        td.totalDataSize = hdr ? x * y * 4 * sizeof(float) : x * y * 4;

        return td;
    }

    TextureData loadCubemapTexture(void* fileData, size_t fileLen, AssetID id)
    {
        // Cubemaps are just json files containing the paths to each individual face
        uint8_t* charData = (uint8_t*)fileData;
        nlohmann::json j = nlohmann::json::parse(charData, charData + fileLen);

        if (!j.is_array() || j.is_discarded() || j.size() != 6)
        {
            logErr("Cubemap %s is not a valid JSON array", AssetDB::idToPath(id).c_str());
            return TextureData { nullptr };
        }

        TextureData faces[6];

        enki::TaskSet loadTask{6, [&](enki::TaskSetPartition range, uint32_t) {
            for (int i = range.start; i < range.end; i++)
            {
                faces[i] = loadTexData(AssetDB::pathToId(j[i].get<std::string>()));
            }
        }};

        g_taskSched.AddTaskSetToPipe(&loadTask);
        g_taskSched.WaitforTask(&loadTask);

        VK::TextureFormat textureFormat = faces[0].format;
        int resolution = faces[0].width;
        int pixelSize = textureFormat == VK::TextureFormat::R32G32B32A32_SFLOAT ? sizeof(float) * 4 : 4;
        if (textureFormat == VK::TextureFormat::BC1_RGBA_SRGB_BLOCK)
        {
            pixelSize = 1;
        }
        size_t faceSize = resolution * resolution * pixelSize;
        if (textureFormat == VK::TextureFormat::BC1_RGBA_SRGB_BLOCK)
        {
            faceSize /= 2;
        }

        if (textureFormat != VK::TextureFormat::R32G32B32A32_SFLOAT && textureFormat != VK::TextureFormat::R8G8B8A8_SRGB)
        {
            logErr("Compressed cubemaps are currently not supported");
            for (int i = 0; i < 6; i++)
            {
                free(faces[i].data);
            }
            return TextureData { nullptr };
        }

        for (int i = 0; i < 6; i++)
        {
            if (faces[i].format != textureFormat)
            {
                logErr("Faces of cubemap %s are not all the same format", AssetDB::idToPath(id).c_str());
                return TextureData { nullptr };
            }

            if (faces[i].width != resolution || faces[i].height != resolution)
            {
                logErr("Faces of cubemap %s differ in resolution", AssetDB::idToPath(id).c_str());
                return TextureData { nullptr };
            }

            //if (loadedFaceSize != faceSize)
            //{
            //    logErr("Faces of cubemap %s differ in data size", AssetDB::idToPath(id).c_str());
            //    return TextureData{ nullptr };
            //}
        }

        TextureData finalData{};
        finalData.isCubemap = true;
        finalData.numMips = 1;
        finalData.totalDataSize = faceSize * 6;
        finalData.height = resolution;
        finalData.width = resolution;
        finalData.name = AssetDB::idToPath(id);
        finalData.format = textureFormat;

        // allocate a single large buffer and put all the faces in there
        uint8_t* finalBuffer = (uint8_t*)malloc(finalData.totalDataSize);

        uint8_t* destination = finalBuffer;
        for (int i = 0; i < 6; i++)
        {
            memcpy(destination, faces[i].data, faceSize);
            free(faces[i].data);
            destination += faceSize;
        }

        finalData.data = finalBuffer;

        return finalData;
    }

    TextureData loadTexData(AssetID id)
    {
        ZoneScoped;

        if (!AssetDB::exists(id))
        {
            return TextureData{nullptr};
        }

        PHYSFS_File* file = AssetDB::openAssetFileRead(id);
        if (!file)
        {
            std::string path = AssetDB::idToPath(id);
            auto errCode = PHYSFS_getLastErrorCode();
            SDL_LogError(worlds::WELogCategoryEngine, "Failed to load texture %s: %s", path.c_str(),
                         PHYSFS_getErrorByCode(errCode));
            return TextureData{nullptr};
        }

        size_t fileLen = PHYSFS_fileLength(file);
        std::vector<uint8_t> fileVec(fileLen);

        PHYSFS_readBytes(file, fileVec.data(), fileLen);
        PHYSFS_close(file);

        std::string ext = AssetDB::getAssetExtension(id);

        if (ext == ".jpg" || ext == ".png" || ext == ".hdr")
        {
            return loadStbTexture(fileVec.data(), fileLen, id);
        }

        if (fileVec[0] == 'H' && fileVec[1] == 'x')
        {
            // old raw crunch texture, load directly
            return loadCrunchTexture(fileVec.data(), fileLen, id);
        }

        if (fileVec[0] == '[')
        {
            return loadCubemapTexture(fileVec.data(), fileLen, id);
        }

        wtex::Header* header = reinterpret_cast<wtex::Header*>(fileVec.data());

        if (!header->verifyMagic())
        {
            logErr("Unknown texture format");
            return TextureData{nullptr};
        }

        if (header->containedFormat == wtex::ContainedFormat::Crunch)
        {
            return loadCrunchTexture(header->getData(), header->dataSize, id);
        }
        else
        {
            logErr("Unsupported texture containedFormat");
            return TextureData{nullptr};
        }
    }
}
