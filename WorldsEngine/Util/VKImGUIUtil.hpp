#pragma once
#include "../Render/RenderInternal.hpp"

namespace worlds {
    namespace VKImGUIUtil {
        void createObjects(const worlds::VulkanHandles* vkCtx);
        void destroyObjects(const worlds::VulkanHandles* vkCtx);
        VkDescriptorSet createDescriptorSetFor(vku::GenericImage& img, const worlds::VulkanHandles* vkCtx);
        void destroyDescriptorSet(VkDescriptorSet ds, const VulkanHandles* handles);
    }
}
