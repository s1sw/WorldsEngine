#include "../Render/Render.hpp"

namespace worlds {
    namespace VKImGUIUtil {
        vk::UniqueDescriptorSetLayout layout;
        vk::Sampler sampler;

        void createObjects(worlds::VulkanHandles& vkCtx) {
            vku::DescriptorSetLayoutMaker dslm;
            dslm.image(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);

            layout = dslm.createUnique(vkCtx.device);

            vku::SamplerMaker sm;
            sampler = sm.create(vkCtx.device);
        }

        void destroyObjects(worlds::VulkanHandles& vkCtx) {
            vkCtx.device.waitIdle();
            layout.reset();
            vkCtx.device.destroySampler(sampler);
        }

        vk::DescriptorSet createDescriptorSetFor(vku::GenericImage& img, const worlds::VulkanHandles& vkCtx) {
            vku::DescriptorSetMaker dsm;
            dsm.layout(*layout);

            auto ds = dsm.create(vkCtx.device, vkCtx.descriptorPool)[0];

            vku::DescriptorSetUpdater dsu;
            dsu.beginDescriptorSet(ds);
            dsu.beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler);
            dsu.image(sampler, img.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            dsu.update(vkCtx.device);

            return ds;
        }

        void destroyDescriptorSet(vk::DescriptorSet ds, const VulkanHandles& handles) {
            handles.device.freeDescriptorSets(handles.descriptorPool, ds);
        }
    }
}
