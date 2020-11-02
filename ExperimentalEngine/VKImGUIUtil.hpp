#pragma once
#include "Render.hpp"

namespace worlds {
    namespace VKImGUIUtil {
        void createObjects(worlds::VulkanCtx& vkCtx);
        void destroyObjects(worlds::VulkanCtx& vkCtx);
        vk::DescriptorSet createDescriptorSetFor(vku::GenericImage& img, worlds::VulkanCtx& vkCtx);
    }
}