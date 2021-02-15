#pragma once
#include "../Render/Render.hpp"

namespace worlds {
    namespace VKImGUIUtil {
        void createObjects(worlds::VulkanHandles& vkCtx);
        void destroyObjects(worlds::VulkanHandles& vkCtx);
        vk::DescriptorSet createDescriptorSetFor(vku::GenericImage& img, const worlds::VulkanHandles& vkCtx);
    }
}
