#pragma once
#include <cstdint>
#include "../../Core/AssetDB.hpp"
#include <string>

namespace worlds {
    struct TextureData {
        uint8_t* data;
        uint32_t width, height;
        uint32_t numMips;
        uint32_t totalDataSize;
        VkFormat format;
        std::string name;
    };

    struct VulkanHandles;

    TextureData loadTexData(AssetID id);
}
