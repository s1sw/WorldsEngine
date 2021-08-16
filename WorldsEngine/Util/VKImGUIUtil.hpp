#pragma once
#include "../Render/RenderInternal.hpp"

namespace worlds {
    namespace VKImGUIUtil {
        void createObjects(const worlds::VulkanHandles* vkCtx);
        void destroyObjects(const worlds::VulkanHandles* vkCtx);
        vk::DescriptorSet createDescriptorSetFor(vku::GenericImage& img, const worlds::VulkanHandles* vkCtx);
        void destroyDescriptorSet(vk::DescriptorSet ds, const VulkanHandles* handles);
    }
}
