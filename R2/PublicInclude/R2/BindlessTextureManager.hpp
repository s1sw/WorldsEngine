#pragma once
#include <stdint.h>
#include <vector>

namespace R2
{
    namespace VK
    {
        class Texture;
        class DescriptorSet;
        struct Handles;
    }

    class BindlessTextureManager
    {
        uint32_t freeLocation = ~0u;
        std::vector<VK::Texture*> textures;
        VK::DescriptorSet* textureDescriptors;
        const VK::Handles* handles;
    public:
        BindlessTextureManager(const VK::Handles* handles);
        ~BindlessTextureManager();
        uint32_t AllocateTextureHandle();
        void SetTextureAt(uint32_t handle, VK::Texture* tex);
        void FreeTextureHandle(uint32_t handle);
        VK::DescriptorSet& GetTextureDescriptorSet();
    };
}