#include "TextureLoader.hpp"
#include "../../Core/Engine.hpp"
#include "tracy/Tracy.hpp"
#include "crn_decomp.h"
#include "stb_image.h"
#include "../../Core/LogCategories.hpp"
#include "../Render.hpp"
#include "../../Core/Fatal.hpp"
#include <physfs.h>
#include <algorithm>

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
        bool hdr = false;
        bool forceLinear = false;
        stbi_uc* dat;

        std::string path = g_assetDB.getAssetPath(id);

        if (path.find("forcelin") != std::string::npos) {
            forceLinear = true;
        }

        if (g_assetDB.getAssetExtension(id) == ".hdr") {
            float* fpDat;
            fpDat = stbi_loadf_from_memory((stbi_uc*)fileData, (int)fileLen, &x, &y, &channelsInFile, 4);
            dat = (stbi_uc*)fpDat;
            hdr = true;
        } else {
            dat = stbi_load_from_memory((stbi_uc*)fileData, (int)fileLen, &x, &y, &channelsInFile, 4);
        }

        if (dat == nullptr) {
            SDL_LogError(worlds::WELogCategoryEngine, "STB Image error\n");
        }

        TextureData td;
        td.data = dat;
        td.numMips = 1;
        td.width = (uint32_t)x;
        td.height = (uint32_t)y;
        if (hdr)
            td.format = vk::Format::eR32G32B32A32Sfloat;
        else if (!forceLinear)
            td.format = vk::Format::eR8G8B8A8Srgb;
        else
            td.format = vk::Format::eR8G8B8A8Unorm;
        td.name = g_assetDB.getAssetPath(id);
        td.totalDataSize = hdr ? x * y * 4 * sizeof(float) : x * y * 4;

        return td;
    }

    TextureData loadCrunchTexture(void* fileData, size_t fileLen, AssetID id) {
        ZoneScoped;
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

        size_t totalDataSize = 0;
        for (uint32_t i = 0; i < texInfo.m_levels; i++) totalDataSize += getCrunchTextureSize(texInfo, i);

        char* data = (char*)std::malloc(totalDataSize);
        size_t currOffset = 0;
        for (uint32_t i = 0; i < texInfo.m_levels; i++) {
            char* dataOffs = &data[currOffset];
            uint32_t dataSize = getCrunchTextureSize(texInfo, i);
            currOffset += dataSize;

            if (!crnd::crnd_unpack_level(context, (void**)&dataOffs, dataSize, getRowPitch(texInfo, i), i))
                fatalErr("Failed to unpack texture");
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

        if (!g_assetDB.hasId(id)) {
            return TextureData{ nullptr };
        }

        PHYSFS_File* file = g_assetDB.openAssetFileRead(id);
        if (!file) {
            std::string path = g_assetDB.getAssetPath(id);
            auto errCode = PHYSFS_getLastErrorCode();
            SDL_LogError(worlds::WELogCategoryEngine, "Failed to load texture %s: %s", path.c_str(), PHYSFS_getErrorByCode(errCode));
            return TextureData{nullptr};
        }

        size_t fileLen = PHYSFS_fileLength(file);
        std::vector<uint8_t> fileVec(fileLen);

        PHYSFS_readBytes(file, fileVec.data(), fileLen);
        PHYSFS_close(file);

        bool crunch = g_assetDB.getAssetExtension(id) == ".crn";

        if (g_assetDB.getAssetExtension(id) == ".vtf") {
            return loadVtfTexture(fileVec.data(), fileLen, id);
        }

        if (!crunch) {
            return loadStbTexture(fileVec.data(), fileLen, id);
        } else {
            return loadCrunchTexture(fileVec.data(), fileLen, id);
        }
    }

    void generateMips(const VulkanHandles& vkCtx, vku::TextureImage2D& t, vk::CommandBuffer cb) {
        auto currLayout = t.layout();
        vk::ImageMemoryBarrier imb;
        imb.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        imb.image = t.image();
        imb.oldLayout = currLayout;
        imb.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        imb.srcAccessMask = vk::AccessFlagBits::eHostWrite;
        imb.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        vk::ImageMemoryBarrier imb2;
        imb2.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 1, t.info().mipLevels - 1, 0, 1 };

        imb2.image = t.image();
        imb2.oldLayout = currLayout;
        imb2.newLayout = vk::ImageLayout::eTransferDstOptimal;
        imb2.srcAccessMask = vk::AccessFlagBits::eHostWrite;
        imb2.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, { imb, imb2 });

        int32_t mipWidth = t.info().extent.width / 2;
        int32_t mipHeight = t.info().extent.height / 2;
        for (uint32_t i = 1; i < t.info().mipLevels; i++) {
            vk::ImageBlit ib;
            ib.dstSubresource = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, i, 0, 1 };
            ib.dstOffsets[0] = vk::Offset3D{ 0, 0, 0 };
            ib.dstOffsets[1] = vk::Offset3D{ mipWidth, mipHeight, 1 };
            ib.srcSubresource = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
            ib.srcOffsets[0] = vk::Offset3D{ 0, 0, 0 };
            ib.srcOffsets[1] = vk::Offset3D{ (int32_t)t.info().extent.width, (int32_t)t.info().extent.height, 1 };

            cb.blitImage(t.image(), vk::ImageLayout::eTransferSrcOptimal, t.image(), vk::ImageLayout::eTransferDstOptimal, ib, vk::Filter::eLinear);

            mipWidth /= 2;
            mipHeight /= 2;
        }

        vk::ImageMemoryBarrier imb3;
        imb3.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 1, t.info().mipLevels - 1, 0, 1 };
        imb3.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        imb3.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imb3.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        imb3.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        imb3.image = t.image();

        vk::ImageMemoryBarrier imb4;
        imb4.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        imb4.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        imb4.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imb4.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        imb4.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        imb4.image = t.image();

        cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlagBits::eByRegion, nullptr, nullptr, { imb3, imb4 });
    }

    void generateMips(const VulkanHandles& vkCtx, vku::TextureImage2D& t) {
        vku::executeImmediately(vkCtx.device, vkCtx.commandPool, vkCtx.device.getQueue(vkCtx.graphicsQueueFamilyIdx, 0),
            [&](vk::CommandBuffer cb) {
                generateMips(vkCtx, t, cb);
            });
    }

    vku::TextureImage2D uploadTextureVk(const VulkanHandles& ctx, TextureData& td) {
        ZoneScoped;
        auto memProps = ctx.physicalDevice.getMemoryProperties();
        bool createMips = td.numMips == 1 && (td.format == vk::Format::eR8G8B8A8Srgb || td.format == vk::Format::eR8G8B8A8Unorm);
        uint32_t maxMips = static_cast<uint32_t>(std::floor(std::log2(std::max(td.width, td.height)))) + 1;

        vku::TextureImage2D tex{
            ctx.device,
            ctx.allocator,
            td.width, td.height,
            createMips ? maxMips : td.numMips, td.format,
            false,
            td.name.empty() ? nullptr : td.name.c_str()
        };

        std::vector<uint8_t> datVec(td.data, td.data + td.totalDataSize);

        tex.upload(ctx.device, ctx.allocator, datVec, ctx.commandPool, memProps, ctx.device.getQueue(ctx.graphicsQueueFamilyIdx, 0), td.numMips);

        if (createMips) {
            generateMips(ctx, tex);
        }

        if (!td.name.empty())
            logMsg("Uploaded %s outside of frame", td.name.c_str());


        return tex;
    }

    // store these in a vector so they can be destroyed after the command buffer has completed
    std::vector<std::vector<vku::GenericBuffer>> tempBuffers;

    void ensureTempVectorExists(uint32_t frameIdx) {
        if (frameIdx >= tempBuffers.size()) {
            tempBuffers.resize(frameIdx + 1);
        }
    }

    void destroyTempTexBuffers(uint32_t frameIdx) {
        ensureTempVectorExists(frameIdx);
        tempBuffers[frameIdx].clear();
    }

    vku::TextureImage2D uploadTextureVk(const VulkanHandles& ctx, TextureData& td, vk::CommandBuffer cb, uint32_t frameIdx) {
        ZoneScoped;
        ensureTempVectorExists(frameIdx);

        bool createMips = td.numMips == 1 && (td.format == vk::Format::eR8G8B8A8Srgb || td.format == vk::Format::eR8G8B8A8Unorm);
        uint32_t maxMips = static_cast<uint32_t>(std::floor(std::log2(std::max(td.width, td.height)))) + 1;

        vku::TextureImage2D tex{
            ctx.device,
            ctx.allocator,
            td.width, td.height,
            createMips ? maxMips : td.numMips, td.format,
            false,
            td.name.empty() ? nullptr : td.name.c_str()
        };

        vku::GenericBuffer stagingBuffer(ctx.device, ctx.allocator, (vk::BufferUsageFlags)vk::BufferUsageFlagBits::eTransferSrc, (vk::DeviceSize)td.totalDataSize, VMA_MEMORY_USAGE_CPU_ONLY, "Texture upload staging buffer");
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

        tempBuffers[frameIdx].push_back(std::move(stagingBuffer));

        if (createMips)
            generateMips(ctx, tex, cb);

        if (!td.name.empty())
            logMsg("Uploaded %s as part of frame", td.name.c_str());

        return tex;
    }
}
