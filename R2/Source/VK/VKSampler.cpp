#include <R2/VKSampler.hpp>
#include <string.h>
#include <volk.h>
#include <R2/VKCore.hpp>

namespace R2::VK
{
    Sampler::Sampler(const Handles* handles, VkSampler sampler)
        : sampler(sampler)
        , handles(handles)
    {}

    VkSampler Sampler::GetNativeHandle()
    {
        return sampler;
    }

    Sampler::~Sampler()
    {
        vkDestroySampler(handles->Device, sampler, handles->AllocCallbacks);
    }

    SamplerBuilder::SamplerBuilder(const Handles* handles)
        : handles(handles)
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
        sci.anisotropyEnable = true;
        sci.maxAnisotropy = 16.0f;

        VkSampler vsamp;
        VKCHECK(vkCreateSampler(handles->Device, &sci, handles->AllocCallbacks, &vsamp));

        return new Sampler(handles, vsamp);
    }
}