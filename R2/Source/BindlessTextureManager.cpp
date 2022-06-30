#include <R2/BindlessTextureManager.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <R2/VKSampler.hpp>
#include <assert.h>

namespace R2
{
    BindlessTextureManager::BindlessTextureManager(VK::Core* core)
        : core(core)
    {
        VK::DescriptorSetLayoutBuilder dslb{core->GetHandles()};

        dslb.Binding(0, VK::DescriptorType::CombinedImageSampler, NUM_TEXTURES, 
            VK::ShaderStage::Vertex | VK::ShaderStage::Fragment | VK::ShaderStage::Compute)
            .PartiallyBound()
            .UpdateAfterBind()
            .VariableDescriptorCount();

        textureDescriptorSetLayout = dslb.Build();

        textureDescriptors = core->CreateDescriptorSet(textureDescriptorSetLayout, NUM_TEXTURES);

        VK::SamplerBuilder sb{core->GetHandles()};
        sampler = sb
            .AddressMode(VK::SamplerAddressMode::Repeat)
            .MinFilter(VK::Filter::Linear)
            .MagFilter(VK::Filter::Linear)
            .MipmapMode(VK::SamplerMipmapMode::Linear)
            .Build();
    }

    BindlessTextureManager::~BindlessTextureManager()
    {
    }

    uint32_t BindlessTextureManager::FindFreeSlot()
    {
        for (uint32_t i = 0; i < NUM_TEXTURES; i++)
        {
            if (!presentTextures[i])
                return i;
        }

        return ~0u;
    }

    uint32_t BindlessTextureManager::AllocateTextureHandle(VK::Texture* tex)
    {
        uint32_t freeSlot = FindFreeSlot();
        textures[freeSlot] = tex;
        presentTextures[freeSlot] = true;
        descriptorsNeedUpdate = true;
        return freeSlot;
    }

    void BindlessTextureManager::SetTextureAt(uint32_t handle, VK::Texture* tex)
    {
        assert(presentTextures[handle]);
        textures[handle] = tex;
        descriptorsNeedUpdate = true;
    }

    void BindlessTextureManager::FreeTextureHandle(uint32_t handle)
    {
        textures[handle] = nullptr;
        presentTextures[handle] = false;
        descriptorsNeedUpdate = true;
    }

    VK::DescriptorSet& BindlessTextureManager::GetTextureDescriptorSet()
    {
        return *textureDescriptors;
    }

    VK::DescriptorSetLayout& BindlessTextureManager::GetTextureDescriptorSetLayout()
    {
        return *textureDescriptorSetLayout;
    }

    void BindlessTextureManager::UpdateDescriptorsIfNecessary()
    {
        if (descriptorsNeedUpdate)
        {
            VK::DescriptorSetUpdater dsu{core->GetHandles(), textureDescriptors};

            for (int i = 0; i < NUM_TEXTURES; i++)
            {
                if (!presentTextures[i]) continue;

                dsu.AddTexture(0, i, VK::DescriptorType::CombinedImageSampler, textures[i], sampler);
            }

            dsu.Update();
            descriptorsNeedUpdate = false;
        }
    }
}