#include "vku.hpp"

namespace vku {
    SamplerMaker::SamplerMaker() {
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

    vku::Sampler SamplerMaker::create(VkDevice device) const {
        VkSampler sampler;
        VKCHECK(vkCreateSampler(device, &s.info, nullptr, &sampler));
        return sampler;
    }
}
