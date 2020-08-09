#pragma once
#include <cstdint>
#include "vku/vku.hpp"
#include "AssetDB.hpp"

struct TextureData {
    uint8_t* data;
    uint32_t width, height;
    uint32_t numMips;
    uint32_t totalDataSize;
    vk::Format format;
    std::string name;
};

struct VulkanCtx;

TextureData loadTexData(AssetID id);
vku::TextureImage2D uploadTextureVk(VulkanCtx& ctx, TextureData& td);