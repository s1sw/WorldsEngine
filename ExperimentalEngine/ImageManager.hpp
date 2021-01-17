#pragma once
#include <vulkan/vulkan.h>

namespace worlds {
    typedef uint32_t AssetID;

    struct Texture {
        VkImage image;
        AssetID id;
    };

    class TextureManager {
    public:

    private:
    };
}