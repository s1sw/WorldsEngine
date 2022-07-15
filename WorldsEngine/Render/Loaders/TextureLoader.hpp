#pragma once
#include <Core/AssetDB.hpp>
#include <R2/VKTexture.hpp>
#include <cstdint>
#include <string>

namespace worlds
{
    struct TextureData
    {
        uint8_t* data;
        uint32_t width, height;
        uint32_t numMips;
        uint32_t totalDataSize;
        R2::VK::TextureFormat format;
        std::string name;
    };

    struct VulkanHandles;

    TextureData loadTexData(AssetID id);
}
