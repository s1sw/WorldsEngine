#pragma once
#include "TextureLoader.hpp"

namespace worlds
{
    struct CubemapData
    {
        TextureData faceData[6];
        std::string debugName;
    };

    CubemapData loadCubemapData(AssetID id);
    void destroyTempCubemapBuffers(uint32_t imageIndex);
}