#include "CubemapLoader.hpp"
#include "tracy/Tracy.hpp"
#include <sajson.h>
#include <SDL_log.h>
#include "../../Core/Engine.hpp"
#include "../RenderInternal.hpp"
#include "../../Util/TimingUtil.hpp"
#include "../../Core/JobSystem.hpp"
#include <algorithm>
#define CRND_HEADER_FILE_ONLY
#include <crn_decomp.h>

namespace worlds {
    CubemapData loadCubemapData(AssetID asset) {
        ZoneScoped;
        PerfTimer timer;

        PHYSFS_File* f = AssetDB::openAssetFileRead(asset);
        size_t fileSize = PHYSFS_fileLength(f);
        char* buffer = (char*)std::malloc(fileSize);
        PHYSFS_readBytes(f, buffer, fileSize);
        PHYSFS_close(f);

        const sajson::document& document = sajson::parse(
            sajson::single_allocation(), sajson::mutable_string_view(fileSize, buffer)
        );

        if (!document.is_valid()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid cubemap document");
            std::free(buffer);
            return CubemapData{};
        }

        const auto& root = document.get_root();

        if (root.get_type() != sajson::TYPE_ARRAY) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid cubemap document");
            std::free(buffer);
        }

        if (root.get_length() != 6) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid cubemap document");
            std::free(buffer);
        }

        CubemapData cd;
        JobList& jl = g_jobSys->getFreeJobList();
        jl.begin();
        for (int i = 0; i < 6; i++) {
            Job j{ [i, &cd, &root] {
                auto val = root.get_array_element(i);
                cd.faceData[i] = loadTexData(AssetDB::pathToId(val.as_string()));
            } };
            jl.addJob(std::move(j));
        }
        jl.end();
        g_jobSys->signalJobListAvailable();
        jl.wait();

        std::free(buffer);

        cd.debugName = AssetDB::idToPath(asset);

        logMsg("Spent %.3fms loading cubemap %s", timer.stopGetMs(), cd.debugName.c_str());
        return cd;
    }

    std::vector<std::vector<vku::GenericBuffer>> cubeTempBuffers;
    std::vector<std::vector<vku::TextureImageCube>> cubeImages;

    void ensureTempVectorExistsCube(uint32_t imageIndex) {
        if (imageIndex >= cubeTempBuffers.size()) {
            cubeTempBuffers.resize(imageIndex + 1);
            cubeImages.resize(imageIndex + 1);
        }
    }

    void destroyTempCubemapBuffers(uint32_t imageIndex) {
        ensureTempVectorExistsCube(imageIndex);
        cubeTempBuffers[imageIndex].clear();
        cubeImages[imageIndex].clear();
    }

    inline uint32_t getNumMips(uint32_t w, uint32_t h) {
        return (uint32_t)(1 + floor(log2(glm::max(w, h))));
    }

    uint32_t getCrunchTextureSize(uint32_t baseW, uint32_t baseH, int mip) {
        const uint32_t width = std::max(1U, baseW >> mip);
        const uint32_t height = std::max(1U, baseH >> mip);
        const uint32_t blocks_x = std::max(1U, (width + 3) >> 2);
        const uint32_t blocks_y = std::max(1U, (height + 3) >> 2);
        const uint32_t row_pitch = blocks_x * crnd::crnd_get_bytes_per_dxt_block(cCRNFmtDXT1);
        const uint32_t total_face_size = row_pitch * blocks_y;

        return total_face_size;
    }

    vku::TextureImageCube uploadCubemapVk(VulkanHandles& ctx, CubemapData& cd, VkCommandBuffer cb, uint32_t imageIndex) {
        PerfTimer pt;
        ZoneScoped;
        ensureTempVectorExistsCube(imageIndex);
        VkFormat firstFormat = cd.faceData->format;
        for (int i = 1; i < 6; i++) {
            if (cd.faceData[i].format != firstFormat) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cubemap has differing face formats!");
                return vku::TextureImageCube{};
            }
        }

        VkFormat newFormat = firstFormat;
        if (newFormat == VK_FORMAT_R8G8B8A8_SRGB) {
            newFormat = VK_FORMAT_R8G8B8A8_UNORM;

            JobList& jl = g_jobSys->getFreeJobList();
            jl.begin();
            for (int i = 0; i < 6; i++) {
                Job j{ [&, i] {
                    for (uint32_t j = 0; j < cd.faceData[i].totalDataSize; j++) {
                        float asFloat = (float)cd.faceData[i].data[j] / 255.0f;
                        asFloat = powf(asFloat, 2.2f);
                        cd.faceData[i].data[j] = asFloat * 255;
                    }
                } };
                jl.addJob(std::move(j));
            }
            jl.end();
            g_jobSys->signalJobListAvailable();
            jl.wait();
        }

        bool needsCopy = newFormat == VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        bool hdr = newFormat == VK_FORMAT_R32G32B32A32_SFLOAT;
        VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        if (!needsCopy) {
            usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
        }

        vku::TextureImageCube tex{
            ctx.device,
            ctx.allocator,
            cd.faceData[0].width, cd.faceData[0].height,
            std::min(getNumMips(cd.faceData[0].width, cd.faceData[0].height), 6u),
            newFormat, false,
            cd.debugName.empty() ? nullptr : ("Initial " + cd.debugName).c_str(), usageFlags
        };

        size_t totalSize = 0;

        for (int i = 0; i < 6; i++) {
            totalSize += cd.faceData[i].totalDataSize;
        }

        void* combinedBuffer = std::malloc(totalSize);
        char* uploadOffset = (char*)combinedBuffer;

        for (int i = 0; i < 6; i++) {
            memcpy(uploadOffset, cd.faceData[i].data, cd.faceData[i].totalDataSize);
            uploadOffset += cd.faceData[i].totalDataSize;
            std::free(cd.faceData[i].data);
        }

        vku::GenericBuffer stagingBuffer(ctx.device, ctx.allocator, (VkBufferUsageFlags)VK_BUFFER_USAGE_TRANSFER_SRC_BIT, (VkDeviceSize)totalSize, VMA_MEMORY_USAGE_CPU_ONLY, "Cubemap staging buffer");
        stagingBuffer.updateLocal(ctx.device, combinedBuffer, totalSize);

        // Copy the staging buffer to the GPU texture and set the layout.
        VkBuffer buf = stagingBuffer.buffer();
        uint32_t offset = 0;
        uint32_t width = tex.extent().width;
        uint32_t height = tex.extent().height;
        for (uint32_t face = 0; face < 6; ++face) {
            tex.copy(cb, buf, 0, face, width, height, 1, offset);
            offset += cd.faceData[face].totalDataSize;
        }

        tex.setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

        // If the cubemap is compressed, we need to blit it to an uncompressed
        // cubemap so we can convolute it.
        vku::TextureImageCube destTex {
            ctx.device,
            ctx.allocator,
            cd.faceData[0].width, cd.faceData[0].height,
            std::min(getNumMips(cd.faceData[0].width, cd.faceData[0].height), 6u),
            hdr ? VK_FORMAT_B10G11R11_UFLOAT_PACK32: VK_FORMAT_R8G8B8A8_UNORM, false,
            cd.debugName.empty() ? nullptr : cd.debugName.c_str(),
            usageFlags | VK_IMAGE_USAGE_STORAGE_BIT
        };

        destTex.setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkImageBlit blit;
        blit.dstSubresource = blit.srcSubresource = VkImageSubresourceLayers {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0, 0, 6
        };

        blit.dstOffsets[0] = blit.srcOffsets[0] = VkOffset3D {
            0, 0, 0
        };

        blit.dstOffsets[1] = blit.srcOffsets[1] = VkOffset3D {
            (int)cd.faceData[0].width, (int)cd.faceData[0].height, 1
        };

        // AMD driver workaround
        // DXT1 compressed textures blit incorrectly and multiplying
        // the source width and height by 4 fixes it.
        //if (ctx.vendor == VKVendor::AMD && needsCopy) {
        //    blit.srcOffsets[1].y *= 4;
        //    blit.srcOffsets[1].x *= 4;
        //}

        vkCmdBlitImage(cb, tex.image(), tex.layout(), destTex.image(), destTex.layout(), 1, &blit, VK_FILTER_LINEAR);
        cubeTempBuffers[imageIndex].push_back(std::move(stagingBuffer));
        cubeImages[imageIndex].push_back(std::move(tex));
        destTex.setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT);

        free(combinedBuffer);
        logMsg("cubemap upload took %fms", pt.stopGetMs());
        return destTex;
    }

    vku::TextureImageCube uploadCubemapVk(VulkanHandles& ctx, CubemapData& cd) {
        vku::TextureImageCube tex;
        VkQueue queue;
        vkGetDeviceQueue(ctx.device, ctx.graphicsQueueFamilyIdx, 0, &queue);
        vku::executeImmediately(ctx.device, ctx.commandPool, queue, [&](VkCommandBuffer cb) {
            tex = uploadCubemapVk(ctx, cd, cb, 0);
        });

        destroyTempCubemapBuffers(0);

        return tex;
    }
}
