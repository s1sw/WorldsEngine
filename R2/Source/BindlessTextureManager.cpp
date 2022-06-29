#include <R2/BindlessTextureManager.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKDescriptorSet.hpp>
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
            .UpdateAfterBind();

        textureDescriptorSetLayout = dslb.Build();

        textureDescriptors = core->CreateDescriptorSet(textureDescriptorSetLayout);
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

                // TODO: Sampler
                dsu.AddTexture(0, i, VK::DescriptorType::CombinedImageSampler, textures[i], nullptr);
            }

            dsu.Update();
        }
    }
}