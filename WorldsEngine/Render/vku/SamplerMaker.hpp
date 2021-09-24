#include "Libs/volk.h"
#include "vkcheck.hpp"

namespace vku {
    /// A class to help build samplers.
    /// Samplers tell the shader stages how to sample an image.
    /// They are used in combination with an image to make a combined image sampler
    /// used by texture() calls in shaders.
    /// They can also be passed to shaders directly for use on multiple image sources.
    class SamplerMaker {
    public:
        /// Default to a very basic sampler.
        SamplerMaker() {
            s.info.magFilter = VK_FILTER_NEAREST;
            s.info.minFilter = VK_FILTER_NEAREST;
            s.info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            s.info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s.info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s.info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s.info.mipLodBias = 0.0f;
            s.info.anisotropyEnable = 0;
            s.info.maxAnisotropy = 0.0f;
            s.info.compareEnable = 0;
            s.info.compareOp = VK_COMPARE_OP_NEVER;
            s.info.minLod = 0;
            s.info.maxLod = 0;
            s.info.borderColor = VkBorderColor{};
            s.info.unnormalizedCoordinates = 0;
        }

        ////////////////////
        //
        // Setters
        //
        SamplerMaker& flags(VkSamplerCreateFlags value) { s.info.flags = value; return *this; }

        /// Set the magnify filter value. (for close textures)
        /// Options are: VK_FILTER_LINEAR and VkFilter::eNearest
        SamplerMaker& magFilter(VkFilter value) { s.info.magFilter = value; return *this; }

        /// Set the minnify filter value. (for far away textures)
        /// Options are: VK_FILTER_LINEAR and VkFilter::eNearest
        SamplerMaker& minFilter(VkFilter value) { s.info.minFilter = value; return *this; }

        /// Set the minnify filter value. (for far away textures)
        /// Options are: VK_SAMPLER_MIPMAP_MODE_LINEAR and VkSamplerMipmapMode::eNearest
        SamplerMaker& mipmapMode(VkSamplerMipmapMode value) { s.info.mipmapMode = value; return *this; }
        SamplerMaker& addressModeU(VkSamplerAddressMode value) { s.info.addressModeU = value; return *this; }
        SamplerMaker& addressModeV(VkSamplerAddressMode value) { s.info.addressModeV = value; return *this; }
        SamplerMaker& addressModeW(VkSamplerAddressMode value) { s.info.addressModeW = value; return *this; }
        SamplerMaker& mipLodBias(float value) { s.info.mipLodBias = value; return *this; }
        SamplerMaker& anisotropyEnable(VkBool32 value) { s.info.anisotropyEnable = value; return *this; }
        SamplerMaker& maxAnisotropy(float value) { s.info.maxAnisotropy = value; return *this; }
        SamplerMaker& compareEnable(VkBool32 value) { s.info.compareEnable = value; return *this; }
        SamplerMaker& compareOp(VkCompareOp value) { s.info.compareOp = value; return *this; }
        SamplerMaker& minLod(float value) { s.info.minLod = value; return *this; }
        SamplerMaker& maxLod(float value) { s.info.maxLod = value; return *this; }
        SamplerMaker& borderColor(VkBorderColor value) { s.info.borderColor = value; return *this; }
        SamplerMaker& unnormalizedCoordinates(VkBool32 value) { s.info.unnormalizedCoordinates = value; return *this; }

        /// Allocate a non self-deleting Sampler.
        VkSampler create(VkDevice device) const {
            VkSampler sampler;
            VKCHECK(vkCreateSampler(device, &s.info, nullptr, &sampler));
            return sampler;
        }

    private:
        struct State {
            VkSamplerCreateInfo info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        };

        State s;
    };
}
