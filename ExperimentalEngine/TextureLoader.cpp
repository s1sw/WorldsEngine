#include "TextureLoader.hpp"
#include "Engine.hpp"
#include "tracy/Tracy.hpp"
#include "crn_decomp.h"
#include "stb_image.h"
#include "LogCategories.hpp"
#include "Render.hpp"

namespace worlds {
    uint32_t getCrunchTextureSize(crnd::crn_texture_info texInfo, int mip) {
        const crn_uint32 width = std::max(1U, texInfo.m_width >> mip);
        const crn_uint32 height = std::max(1U, texInfo.m_height >> mip);
        const crn_uint32 blocks_x = std::max(1U, (width + 3) >> 2);
        const crn_uint32 blocks_y = std::max(1U, (height + 3) >> 2);
        const crn_uint32 row_pitch = blocks_x * crnd::crnd_get_bytes_per_dxt_block(texInfo.m_format);
        const crn_uint32 total_face_size = row_pitch * blocks_y;

        return total_face_size;
    }

    uint32_t getRowPitch(crnd::crn_texture_info texInfo, int mip) {
        const crn_uint32 width = std::max(1U, texInfo.m_width >> mip);
        const crn_uint32 height = std::max(1U, texInfo.m_height >> mip);
        const crn_uint32 blocks_x = std::max(1U, (width + 3) >> 2);
        const crn_uint32 row_pitch = blocks_x * crnd::crnd_get_bytes_per_dxt_block(texInfo.m_format);

        return row_pitch;
    }

    inline int getNumMips(int w, int h) {
        return (int)(1 + floor(log2(glm::max(w, h))));
    }

    TextureData loadStbTexture(void* fileData, size_t fileLen, AssetID id) {
        ZoneScoped;
        int x, y, channelsInFile;
        stbi_uc* dat = stbi_load_from_memory((stbi_uc*)fileData, (int)fileLen, &x, &y, &channelsInFile, 4);

        if (dat == nullptr) {
            SDL_LogError(worlds::WELogCategoryEngine, "STB Image error\n");
        }

        TextureData td;
        td.data = dat;
        td.numMips = 1;
        td.width = (uint32_t)x;
        td.height = (uint32_t)y;
        td.format = vk::Format::eR8G8B8A8Srgb;
        td.name = g_assetDB.getAssetPath(id);
        td.totalDataSize = x * y * 4;

        return td;
    }

    TextureData loadCrunchTexture(void* fileData, size_t fileLen, AssetID id) {
        ZoneScoped
            bool isSRGB = true;

        crnd::crn_texture_info texInfo;

        if (!crnd::crnd_get_texture_info(fileData, (uint32_t)fileLen, &texInfo))
            return TextureData{};

        crnd::crnd_unpack_context context = crnd::crnd_unpack_begin(fileData, (uint32_t)fileLen);

        crn_format fundamentalFormat = crnd::crnd_get_fundamental_dxt_format(texInfo.m_format);

        vk::Format format{};

        switch (fundamentalFormat) {
        case crn_format::cCRNFmtDXT1:
            format = isSRGB ? vk::Format::eBc1RgbaSrgbBlock : vk::Format::eBc1RgbaUnormBlock;
            break;
        case crn_format::cCRNFmtDXT5:
            format = isSRGB ? vk::Format::eBc3SrgbBlock : vk::Format::eBc3UnormBlock;
            break;
        case crn_format::cCRNFmtDXN_XY:
            format = vk::Format::eBc5UnormBlock;
            break;
        default:
            format = vk::Format::eUndefined;
            break;
        }

        uint32_t x = texInfo.m_width;
        uint32_t y = texInfo.m_height;
        uint32_t pitch = getRowPitch(texInfo, 0);

        size_t totalDataSize = 0;
        for (uint32_t i = 0; i < texInfo.m_levels; i++) totalDataSize += getCrunchTextureSize(texInfo, i);

        char* data = (char*)std::malloc(totalDataSize);
        size_t currOffset = 0;
        for (uint32_t i = 0; i < texInfo.m_levels; i++) {
            char* dataOffs = &data[currOffset];
            uint32_t dataSize = getCrunchTextureSize(texInfo, i);
            currOffset += dataSize;

            if (!crnd::crnd_unpack_level(context, (void**)&dataOffs, dataSize, getRowPitch(texInfo, i), i))
                __debugbreak();
        }

        uint32_t numMips = texInfo.m_levels;

        crnd::crnd_unpack_end(context);

        TextureData td;
        td.data = (uint8_t*)data;
        td.numMips = numMips;
        td.width = x;
        td.height = y;
        td.format = format;
        td.name = g_assetDB.getAssetPath(id);
        td.totalDataSize = (uint32_t)totalDataSize;

        return td;
    }

    TextureData loadTexData(AssetID id) {
        ZoneScoped;

        PHYSFS_File* file = g_assetDB.openAssetFileRead(id);
        if (!file) {
            SDL_LogError(worlds::WELogCategoryEngine, "Failed to load texture");
            return TextureData{};
        }

        size_t fileLen = PHYSFS_fileLength(file);
        std::vector<uint8_t> fileVec(fileLen);

        PHYSFS_readBytes(file, fileVec.data(), fileLen);
        PHYSFS_close(file);

        bool crunch = g_assetDB.getAssetExtension(id) == ".crn";

        if (!crunch) {
            return loadStbTexture(fileVec.data(), fileLen, id);
        } else {
            return loadCrunchTexture(fileVec.data(), fileLen, id);
        }
    }

    vku::TextureImage2D uploadTextureVk(VulkanCtx& ctx, TextureData& td) {
        ZoneScoped;
        auto memProps = ctx.physicalDevice.getMemoryProperties();
        vku::TextureImage2D tex{
            ctx.device,
            memProps,
            td.width, td.height,
            td.numMips, td.format,
            false,
            td.name.empty() ? nullptr : td.name.c_str()
        };

        std::vector<uint8_t> datVec(td.data, td.data + td.totalDataSize);

        tex.upload(ctx.device, ctx.allocator, datVec, ctx.commandPool, memProps, ctx.device.getQueue(ctx.graphicsQueueFamilyIdx, 0));

        return tex;
    }

    // store these in a vector so they can be destroyed after the command buffer has completed
    std::vector<std::vector<vku::GenericBuffer>> tempBuffers;

    void ensureTempVectorExists(uint32_t imageIndex) {
        if (imageIndex >= tempBuffers.size()) {
            tempBuffers.resize(imageIndex + 1);
        }
    }

    void destroyTempTexBuffers(uint32_t imageIndex) {
        ensureTempVectorExists(imageIndex);
        tempBuffers[imageIndex].clear();
    }

    vku::TextureImage2D uploadTextureVk(VulkanCtx& ctx, TextureData& td, vk::CommandBuffer cb, uint32_t imageIndex) {
        ZoneScoped;
        ensureTempVectorExists(imageIndex);
        auto memProps = ctx.physicalDevice.getMemoryProperties();
        vku::TextureImage2D tex{
            ctx.device,
            memProps,
            td.width, td.height,
            td.numMips, td.format,
            false,
            td.name.empty() ? nullptr : td.name.c_str()
        };

        vku::GenericBuffer stagingBuffer(ctx.device, ctx.allocator, (vk::BufferUsageFlags)vk::BufferUsageFlagBits::eTransferSrc, (vk::DeviceSize)td.totalDataSize, VMA_MEMORY_USAGE_CPU_ONLY);
        stagingBuffer.updateLocal(ctx.device, td.data, td.totalDataSize);

        // Copy the staging buffer to the GPU texture and set the layout.
        {
            auto bp = vku::getBlockParams(td.format);
            vk::Buffer buf = stagingBuffer.buffer();
            uint32_t offset = 0;
            for (uint32_t mipLevel = 0; mipLevel != td.numMips; ++mipLevel) {
                auto width = vku::mipScale(td.width, mipLevel);
                auto height = vku::mipScale(td.height, mipLevel);
                for (uint32_t face = 0; face != 1; ++face) {
                    tex.copy(cb, buf, mipLevel, face, width, height, 1, offset);
                    offset += ((bp.bytesPerBlock + 3) & ~3) * ((width / bp.blockWidth) * (height / bp.blockHeight));
                }
            }
            tex.setLayout(cb, vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        tempBuffers[imageIndex].push_back(std::move(stagingBuffer));

        return tex;
    }
}