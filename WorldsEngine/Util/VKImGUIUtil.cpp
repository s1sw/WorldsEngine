#include "../Render/RenderInternal.hpp"
#include "Render/vku/SamplerMaker.hpp"

namespace worlds {
    namespace VKImGUIUtil {
        VkDescriptorSetLayout layout;
        VkSampler sampler;

        void createObjects(const worlds::VulkanHandles* vkCtx) {
            vku::DescriptorSetLayoutMaker dslm;
            dslm.image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);

            layout = dslm.create(vkCtx->device);

            vku::SamplerMaker sm;
            sampler = sm.create(vkCtx->device);
        }

        void destroyObjects(const worlds::VulkanHandles* vkCtx) {
            vkDeviceWaitIdle(vkCtx->device);
            vkDestroyDescriptorSetLayout(vkCtx->device, layout, nullptr);
            vkDestroySampler(vkCtx->device, sampler, nullptr);
        }

        VkDescriptorSet createDescriptorSetFor(vku::GenericImage& img, const worlds::VulkanHandles* vkCtx) {
            vku::DescriptorSetMaker dsm;
            dsm.layout(layout);

            auto ds = dsm.create(vkCtx->device, vkCtx->descriptorPool)[0];

            vku::DescriptorSetUpdater dsu;
            dsu.beginDescriptorSet(ds);
            dsu.beginImages(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            dsu.image(sampler, img.imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            dsu.update(vkCtx->device);

            return ds;
        }

        void destroyDescriptorSet(VkDescriptorSet ds, const VulkanHandles* handles) {
            vkDeviceWaitIdle(handles->device);
            vkFreeDescriptorSets(handles->device, handles->descriptorPool, 1, &ds);
        }
    }
}
