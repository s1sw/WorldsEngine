#pragma once
#include <stdint.h>
#include <stddef.h>

namespace worlds {
    typedef uint32_t AssetID;

    enum class RawTextureFormat {
        RGBA8,
        RGBA32F
    };

    struct RawTextureData {
        RawTextureFormat format;
        int width;
        int height;
        void* data;
        size_t totalDataSize;
    };

    class RawTextureLoader {
    public:
        static bool loadRawTexture(AssetID id, RawTextureData& texData);
    private:
        static bool loadStbTexture(void* fileData, size_t fileLen, AssetID id, RawTextureData& texData);
        static bool loadExrTexture(void* fileData, size_t fileLen, AssetID id, RawTextureData& texData);
    };
}
