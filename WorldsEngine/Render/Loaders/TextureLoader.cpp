#define CRND_HEADER_FILE_ONLY
#include "TextureLoader.hpp"
#include "../../Core/Engine.hpp"
#include "tracy/Tracy.hpp"
#include "crn_decomp.h"
#include "stb_image.h"
#include "../../Core/LogCategories.hpp"
#include "../RenderInternal.hpp"
#include "../../Core/Fatal.hpp"
#include <physfs.h>
#include <algorithm>

namespace worlds {
    std::mutex vkMutex;

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

        std::string path = AssetDB::idToPath(id);

        if (path.find("forcelin") != std::string::npos) {
            forceLinear = true;
        }

        if (AssetDB::getAssetExtension(id) == ".hdr") {
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
            td.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        else if (!forceLinear)
            td.format = VK_FORMAT_R8G8B8A8_SRGB;
        else
            td.format = VK_FORMAT_R8G8B8A8_UNORM;
        td.name = AssetDB::idToPath(id);
        td.totalDataSize = hdr ? x * y * 4 * sizeof(float) : x * y * 4;

        return td;
    }

    TextureData loadCrunchTexture(void* fileData, size_t fileLen, AssetID id) {
        ZoneScoped;

        crnd::crn_texture_info texInfo;

        if (!crnd::crnd_get_texture_info(fileData, (uint32_t)fileLen, &texInfo))
            return TextureData{};

        bool isSRGB = texInfo.m_userdata0;
        crnd::crnd_unpack_context context = crnd::crnd_unpack_begin(fileData, (uint32_t)fileLen);

        crn_format fundamentalFormat = crnd::crnd_get_fundamental_dxt_format(texInfo.m_format);

        VkFormat format{};

        switch (fundamentalFormat) {
        case crn_format::cCRNFmtDXT1:
            format = isSRGB ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK : VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            break;
        case crn_format::cCRNFmtDXT5:
            format = isSRGB ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
            break;
        case crn_format::cCRNFmtDXN_XY:
            format = VK_FORMAT_BC5_UNORM_BLOCK;
            break;
        default:
            format = VK_FORMAT_UNDEFINED;
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
        td.name = AssetDB::idToPath(id);
        td.totalDataSize = (uint32_t)totalDataSize;

        return td;
    }

    TextureData loadTexData(AssetID id) {
        ZoneScoped;

        if (!AssetDB::exists(id)) {
            return TextureData{ nullptr };
        }

        PHYSFS_File* file = AssetDB::openAssetFileRead(id);
        if (!file) {
            std::string path = AssetDB::idToPath(id);
            auto errCode = PHYSFS_getLastErrorCode();
            SDL_LogError(worlds::WELogCategoryEngine, "Failed to load texture %s: %s", path.c_str(), PHYSFS_getErrorByCode(errCode));
            return TextureData{nullptr};
        }

        size_t fileLen = PHYSFS_fileLength(file);
        std::vector<uint8_t> fileVec(fileLen);

        PHYSFS_readBytes(file, fileVec.data(), fileLen);
        PHYSFS_close(file);

        std::string ext = AssetDB::getAssetExtension(id);

        bool crunch = ext == ".crn" || ext == ".wtex";

        if (AssetDB::getAssetExtension(id) == ".vtf") {
            return loadVtfTexture(fileVec.data(), fileLen, id);
        }

        if (!crunch) {
            return loadStbTexture(fileVec.data(), fileLen, id);
        } else {
            return loadCrunchTexture(fileVec.data(), fileLen, id);
        }
    }

    void generateMips(const VulkanHandles& vkCtx, vku::TextureImage2D& t, VkCommandBuffer cb) {
        auto currLayout = t.layout();
        VkImageMemoryBarrier imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imb.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        imb.image = t.image();
        imb.oldLayout = currLayout;
        imb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imb.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        imb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        VkImageMemoryBarrier imb2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imb2.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 1, t.info().mipLevels - 1, 0, 1 };

        imb2.image = t.image();
        imb2.oldLayout = currLayout;
        imb2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imb2.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        imb2.dstAccessMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

        VkImageMemoryBarrier barriers[2] = { imb, imb2 };
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 
            0, nullptr, 
            0, nullptr, 
            2, barriers);

        int32_t mipWidth = t.info().extent.width / 2;
        int32_t mipHeight = t.info().extent.height / 2;
        for (uint32_t i = 1; i < t.info().mipLevels; i++) {
            VkImageBlit ib;
            ib.dstSubresource = VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 };
            ib.dstOffsets[0] = VkOffset3D{ 0, 0, 0 };
            ib.dstOffsets[1] = VkOffset3D{ mipWidth, mipHeight, 1 };
            ib.srcSubresource = VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            ib.srcOffsets[0] = VkOffset3D{ 0, 0, 0 };
            ib.srcOffsets[1] = VkOffset3D{ (int32_t)t.info().extent.width, (int32_t)t.info().extent.height, 1 };

            vkCmdBlitImage(cb, t.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, t.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &ib, VK_FILTER_LINEAR);

            mipWidth /= 2;
            mipHeight /= 2;
        }

        VkImageMemoryBarrier imb3{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imb3.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 1, t.info().mipLevels - 1, 0, 1 };
        imb3.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imb3.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imb3.srcAccessMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        imb3.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imb3.image = t.image();

        VkImageMemoryBarrier imb4{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imb4.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        imb4.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imb4.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imb4.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imb4.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imb4.image = t.image();

        VkImageMemoryBarrier barriers34[2] = { imb3, imb4 };


        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, 
            0, nullptr, 
            0, nullptr, 
            2, barriers34);
        t.setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    }

    void generateMips(const VulkanHandles& vkCtx, vku::TextureImage2D& t) {
        VkQueue queue;
        vkGetDeviceQueue(vkCtx.device, vkCtx.graphicsQueueFamilyIdx, 0, &queue);
        vku::executeImmediately(vkCtx.device, vkCtx.commandPool, queue,
            [&](VkCommandBuffer cb) {
                generateMips(vkCtx, t, cb);
            });
    }

    vku::TextureImage2D uploadTextureVk(const VulkanHandles& ctx, TextureData& td, bool genMips) {
        ZoneScoped;
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(ctx.physicalDevice, &memProps);
        bool createMips = td.numMips == 1 && (td.format == VK_FORMAT_R8G8B8A8_SRGB || td.format == VK_FORMAT_R8G8B8A8_UNORM);
        uint32_t maxMips = static_cast<uint32_t>(std::floor(std::log2(std::min(td.width, td.height)))) + 1;

        vkMutex.lock();
        vku::TextureImage2D tex{
            ctx.device,
            ctx.allocator,
            td.width, td.height,
            createMips ? maxMips : td.numMips, td.format,
            false,
            td.name.empty() ? nullptr : td.name.c_str()
        };

        std::vector<uint8_t> datVec(td.data, td.data + td.totalDataSize);

        VkQueue queue;
        vkGetDeviceQueue(ctx.device, ctx.graphicsQueueFamilyIdx, 0, &queue);

        tex.upload(ctx.device, ctx.allocator, datVec, ctx.commandPool, memProps, queue, td.numMips);

        if (createMips && genMips) {
            generateMips(ctx, tex);
        }

        vkMutex.unlock();

        if (!td.name.empty())
            logMsg("Uploaded %s (%ix%i) outside of frame", td.name.c_str(), td.width, td.height);

        return tex;
    }

    // store these in a vector so they can be destroyed after the command buffer has completed
    std::vector<std::vector<vku::GenericBuffer>> tempBuffers;
    std::mutex tempBufMutex;

    void ensureTempVectorExists(uint32_t frameIdx) {
        tempBufMutex.lock();
        if (frameIdx >= tempBuffers.size()) {
            tempBuffers.resize(frameIdx + 1);
        }
        tempBufMutex.unlock();
    }

    void destroyTempTexBuffers(uint32_t frameIdx) {
        ensureTempVectorExists(frameIdx);
        tempBufMutex.lock();
        tempBuffers[frameIdx].clear();
        tempBufMutex.unlock();
    }

    vku::TextureImage2D uploadTextureVk(const VulkanHandles& ctx, TextureData& td, VkCommandBuffer cb, uint32_t frameIdx) {
        ZoneScoped;
        ensureTempVectorExists(frameIdx);

        bool createMips = td.numMips == 1 && (td.format == VK_FORMAT_R8G8B8A8_SRGB || td.format == VK_FORMAT_R8G8B8A8_UNORM);
        uint32_t maxMips = static_cast<uint32_t>(std::floor(std::log2(std::max(td.width, td.height)))) + 1;

        vkMutex.lock();
        vku::TextureImage2D tex{
            ctx.device,
            ctx.allocator,
            td.width, td.height,
            createMips ? maxMips : td.numMips, td.format,
            false,
            td.name.empty() ? nullptr : td.name.c_str()
        };

        vku::GenericBuffer stagingBuffer(ctx.device, ctx.allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, (VkDeviceSize)td.totalDataSize, VMA_MEMORY_USAGE_CPU_ONLY, "Texture upload staging buffer");
        stagingBuffer.updateLocal(ctx.device, td.data, td.totalDataSize);

        // Copy the staging buffer to the GPU texture and set the layout.
        {
            auto bp = vku::getBlockParams(td.format);
            VkBuffer buf = stagingBuffer.buffer();
            uint32_t offset = 0;
            for (uint32_t mipLevel = 0; mipLevel != td.numMips; ++mipLevel) {
                auto width = vku::mipScale(td.width, mipLevel);
                auto height = vku::mipScale(td.height, mipLevel);
                for (uint32_t face = 0; face != 1; ++face) {
                    tex.copy(cb, buf, mipLevel, face, width, height, 1, offset);
                    offset += ((bp.bytesPerBlock + 3) & ~3) * ((width / bp.blockWidth) * (height / bp.blockHeight));
                }
            }
            tex.setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        }

        tempBufMutex.lock();
        tempBuffers[frameIdx].push_back(std::move(stagingBuffer));
        tempBufMutex.unlock();

        if (createMips)
            generateMips(ctx, tex, cb);
        vkMutex.unlock();

        if (!td.name.empty())
            logMsg("Uploaded %s (%ix%i) as part of frame (handle is %zu)", td.name.c_str(), td.width, td.height, tex.imageView());

        return tex;
    }
}
