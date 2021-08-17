#include "RenderInternal.hpp"

namespace worlds {
    // adapted from https://zeux.io/2019/07/17/serializing-pipeline-cache/
    struct PipelineCacheDataHeader {
        PipelineCacheDataHeader() {
            magic[0] = 'W';
            magic[1] = 'P';
            magic[2] = 'L';
            magic[3] = 'C';
        }

        uint8_t magic[4];    // an arbitrary magic header to make sure this is actually our file
        uint32_t dataSize; // equal to *pDataSize returned by vkGetPipelineCacheData

        uint32_t vendorID;      // equal to VkPhysicalDeviceProperties::vendorID
        uint32_t deviceID;      // equal to VkPhysicalDeviceProperties::deviceID
        uint32_t driverVersion; // equal to VkPhysicalDeviceProperties::driverVersion

        uint8_t uuid[VK_UUID_SIZE]; // equal to VkPhysicalDeviceProperties::pipelineCacheUUID
    };

    std::string getPipelineCachePath(const VkPhysicalDeviceProperties& physDevProps) {
        char* base_path = SDL_GetPrefPath("Someone Somewhere", "Worlds Engine");
        std::string ret = base_path + std::string(physDevProps.deviceName) + "-pipelinecache.wplc";

        SDL_free(base_path);

        return ret;
    }

    void PipelineCacheSerializer::loadPipelineCache(
        const VkPhysicalDeviceProperties& physDevProps,
        VkPipelineCacheCreateInfo& pcci) {
        std::string pipelineCachePath = getPipelineCachePath(physDevProps);

        pcci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

        FILE* f = fopen(pipelineCachePath.c_str(), "rb");

        if (f) {
            PipelineCacheDataHeader cacheDataHeader;
            size_t readBytes = fread(&cacheDataHeader, 1, sizeof(cacheDataHeader), f);

            if (readBytes != sizeof(cacheDataHeader)) {
                logErr(WELogCategoryRender, "Error while loading pipeline cache: read %i out of %i header bytes", readBytes, sizeof(cacheDataHeader));
                fclose(f);
                return;
            }

            if (cacheDataHeader.deviceID != physDevProps.deviceID ||
                cacheDataHeader.driverVersion != physDevProps.driverVersion ||
                cacheDataHeader.vendorID != physDevProps.vendorID) {
                logErr(WELogCategoryRender, "Error while loading pipeline cache: device properties didn't match");
                fclose(f);
                pcci.pInitialData = nullptr;
                pcci.initialDataSize = 0;
                return;
            }

            pcci.pInitialData = std::malloc(cacheDataHeader.dataSize);
            pcci.initialDataSize = cacheDataHeader.dataSize;
            readBytes = fread((void*)pcci.pInitialData, 1, cacheDataHeader.dataSize, f);

            if (readBytes != cacheDataHeader.dataSize) {
                logErr(WELogCategoryRender, "Error while loading pipeline cache: couldn't read data");
                std::free((void*)pcci.pInitialData);
                pcci.pInitialData = nullptr;
                pcci.initialDataSize = 0;
                fclose(f);
                return;
            }

            logMsg(WELogCategoryRender, "Loaded pipeline cache: %i bytes", cacheDataHeader.dataSize);

            fclose(f);
        }
    }

    void PipelineCacheSerializer::savePipelineCache(
        const VkPhysicalDeviceProperties& physDevProps,
        const VkPipelineCache& cache,
        const VkDevice& dev) {
        size_t pipelineCacheSize;
        VKCHECK(vkGetPipelineCacheData(dev, cache, &pipelineCacheSize, nullptr));

        void* dat = malloc(pipelineCacheSize);
        VKCHECK(vkGetPipelineCacheData(dev, cache, &pipelineCacheSize, dat));

        PipelineCacheDataHeader pipelineCacheHeader{};
        pipelineCacheHeader.dataSize = pipelineCacheSize;
        pipelineCacheHeader.vendorID = physDevProps.vendorID;
        pipelineCacheHeader.deviceID = physDevProps.deviceID;
        pipelineCacheHeader.driverVersion = physDevProps.driverVersion;
        memcpy(pipelineCacheHeader.uuid, physDevProps.pipelineCacheUUID, VK_UUID_SIZE);

        std::string pipelineCachePath = getPipelineCachePath(physDevProps);
        FILE* f = fopen(pipelineCachePath.c_str(), "wb");
        fwrite(&pipelineCacheHeader, sizeof(pipelineCacheHeader), 1, f);

        fwrite(dat, pipelineCacheSize, 1, f);
        fclose(f);

        free(dat);

        logMsg(WELogCategoryRender, "Saved pipeline cache to %s", pipelineCachePath.c_str());
    }
}