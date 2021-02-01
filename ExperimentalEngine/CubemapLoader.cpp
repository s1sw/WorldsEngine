#include "CubemapLoader.hpp"
#include "tracy/Tracy.hpp"
#include <sajson.h>
#include <SDL_log.h>
#include "Engine.hpp"
#include "Render.hpp"
#include "TimingUtil.hpp"
#include "JobSystem.hpp"
#include <algorithm>

namespace worlds {
    CubemapData loadCubemapData(AssetID asset) {
        ZoneScoped;
        PerfTimer timer;

        PHYSFS_File* f = g_assetDB.openAssetFileRead(asset);
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
                cd.faceData[i] = loadTexData(g_assetDB.addOrGetExisting(val.as_string()));
            } };
            jl.addJob(std::move(j));
        }
        jl.end();
        g_jobSys->signalJobListAvailable();
        jl.wait();

        std::free(buffer);

        cd.debugName = g_assetDB.getAssetPath(asset);

        logMsg("Spent %.3fms loading cubemap %s", timer.stopGetMs(), cd.debugName.c_str());
        return cd;
    }

    std::vector<std::vector<vku::GenericBuffer>> cubeTempBuffers;

    void ensureTempVectorExistsCube(uint32_t imageIndex) {
        if (imageIndex >= cubeTempBuffers.size()) {
            cubeTempBuffers.resize(imageIndex + 1);
        }
    }

    void destroyTempCubemapBuffers(uint32_t imageIndex) {
        ensureTempVectorExistsCube(imageIndex);
        cubeTempBuffers[imageIndex].clear();
    }

    inline uint32_t getNumMips(uint32_t w, uint32_t h) {
        return (uint32_t)(1 + floor(log2(glm::max(w, h))));
    }

    vku::TextureImageCube uploadCubemapVk(VulkanHandles& ctx, CubemapData& cd, vk::CommandBuffer cb, uint32_t imageIndex) {
        PerfTimer pt;
        ZoneScoped;
        ensureTempVectorExistsCube(imageIndex);
        vk::Format firstFormat = cd.faceData->format;
        for (int i = 1; i < 6; i++) {
            if (cd.faceData[i].format != firstFormat) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cubemap has differing face formats!");
                return vku::TextureImageCube{};
            }
        }

        vk::Format newFormat = firstFormat;

        if (newFormat == vk::Format::eR8G8B8A8Srgb)
            newFormat = vk::Format::eR8G8B8A8Unorm;

        if (newFormat == vk::Format::eR8G8B8A8Unorm) {
            JobList& jl = g_jobSys->getFreeJobList();
            jl.begin();
            for (int i = 0; i < 6; i++) {
                Job j{ [&, i] {
                    for (int j = 0; j < cd.faceData[i].totalDataSize; j++) {
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

        vku::TextureImageCube tex{
            ctx.device,
            ctx.allocator,
            cd.faceData[0].width, cd.faceData[0].height,
            std::min(getNumMips(cd.faceData[0].width, cd.faceData[0].height), 5u), 
            newFormat, false,
            cd.debugName.empty() ? nullptr : cd.debugName.c_str()
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

        vku::GenericBuffer stagingBuffer(ctx.device, ctx.allocator, (vk::BufferUsageFlags)vk::BufferUsageFlagBits::eTransferSrc, (vk::DeviceSize)totalSize, VMA_MEMORY_USAGE_CPU_ONLY, "Cubemap staging buffer");
        stagingBuffer.updateLocal(ctx.device, combinedBuffer, totalSize);

        // Copy the staging buffer to the GPU texture and set the layout.
        auto bp = vku::getBlockParams(tex.format());
        vk::Buffer buf = stagingBuffer.buffer();
        uint32_t offset = 0;
        uint32_t width = tex.extent().width;
        uint32_t height = tex.extent().height;
        for (uint32_t face = 0; face != 6; ++face) {
            tex.copy(cb, buf, 0, face, width, height, 1, offset);
            offset += ((bp.bytesPerBlock + 3) & ~3) * ((width / bp.blockWidth) * (height / bp.blockHeight));
        }

        tex.setLayout(cb, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);

        cubeTempBuffers[imageIndex].push_back(std::move(stagingBuffer));

        logMsg("cubemap upload took %fms", pt.stopGetMs());

        return tex;
    }

    vku::TextureImageCube uploadCubemapVk(VulkanHandles& ctx, CubemapData& cd) {
        vku::TextureImageCube tex;
        vku::executeImmediately(ctx.device, ctx.commandPool, ctx.device.getQueue(ctx.graphicsQueueFamilyIdx, 0), [&](vk::CommandBuffer cb) {
            tex = uploadCubemapVk(ctx, cd, cb, 0);
            });

        destroyTempCubemapBuffers(0);

        return tex;
    }
}
