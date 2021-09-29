#pragma once
#include <cstdint>
#include "../vku/vku.hpp"
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
    TextureData loadVtfTexture(void* fileData, size_t fileLen, AssetID id);
    vku::TextureImage2D uploadTextureVk(const VulkanHandles& ctx, TextureData& td, bool generateMips = true);
    vku::TextureImage2D uploadTextureVk(const VulkanHandles& ctx, TextureData& td, VkCommandBuffer cb, uint32_t imageIndex);
    void destroyTempTexBuffers(uint32_t imageIndex);
}
