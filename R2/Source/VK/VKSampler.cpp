#include <R2/VKSampler.hpp>
#include <string.h>
#include <volk.h>
#include <R2/VKCore.hpp>
#include <R2/VKDeletionQueue.hpp>

namespace R2::VK
{
    Sampler::Sampler(Core* core, VkSampler sampler)
        : sampler(sampler)
        , core(core)
    {}

    VkSampler Sampler::GetNativeHandle()
    {
        return sampler;
    }

    Sampler::~Sampler()
    {
        DeletionQueue* dq = core->perFrameResources[core->frameIndex].DeletionQueue;
        dq->QueueObjectDeletion(sampler, VK_OBJECT_TYPE_SAMPLER);
    }

    SamplerBuilder::SamplerBuilder(Core* core)
        : core(core)
        , ci{}
    {}

    SamplerBuilder& SamplerBuilder::MagFilter(Filter filt)
    {
        ci.magFilter = filt;
        
        return *this;
    }

    SamplerBuilder& SamplerBuilder::MinFilter(Filter filt)
    {
        ci.minFilter = filt;

        return *this;
    }

    SamplerBuilder& SamplerBuilder::MipmapMode(SamplerMipmapMode mode)
    {
        ci.mipmapMode = mode;

        return *this;
    }

    SamplerBuilder& SamplerBuilder::AddressMode(SamplerAddressMode mode)
    {
        ci.addressModeU = mode;
        ci.addressModeV = mode;
        ci.addressModeW = mode;

        return *this;
    }

    Sampler* SamplerBuilder::Build()
    {
        VkSamplerCreateInfo sci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        memcpy(&sci.magFilter, &ci, sizeof(SamplerCreateInfo));

        sci.minLod = 0.0f;
        sci.maxLod = VK_LOD_CLAMP_NONE;
        sci.anisotropyEnable = false;
        sci.maxAnisotropy = 8.0f;

        VkSampler vsamp;
        VKCHECK(vkCreateSampler(core->GetHandles()->Device, &sci, core->GetHandles()->AllocCallbacks, &vsamp));

        return new Sampler(core, vsamp);
    }
}