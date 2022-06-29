#pragma once
#include <stdint.h>
#include <array>
#include <bitset>

namespace R2
{
    namespace VK
    {
        class Core;
        class Texture;
        class DescriptorSet;
        class DescriptorSetLayout;
    }

    class BindlessTextureManager
    {
        static const uint32_t NUM_TEXTURES = 64;

        std::array<VK::Texture*, NUM_TEXTURES> textures;
        std::bitset<NUM_TEXTURES> presentTextures;

        VK::DescriptorSet* textureDescriptors;
        VK::DescriptorSetLayout* textureDescriptorSetLayout;
        VK::Core* core;
        bool descriptorsNeedUpdate = false;

        uint32_t FindFreeSlot();
    public:
        BindlessTextureManager(VK::Core* core);
        ~BindlessTextureManager();

        uint32_t AllocateTextureHandle(VK::Texture* tex);
        void SetTextureAt(uint32_t handle, VK::Texture* tex);
        void FreeTextureHandle(uint32_t handle);

        VK::DescriptorSet& GetTextureDescriptorSet();
        VK::DescriptorSetLayout& GetTextureDescriptorSetLayout();
        void UpdateDescriptorsIfNecessary();
    };
}