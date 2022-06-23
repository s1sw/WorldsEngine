#pragma once
#include <cstdint>
#include <Core/AssetDB.hpp>
#include <string>
#include <R2/VKTexture.hpp>

namespace worlds {
    struct TextureData {
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
