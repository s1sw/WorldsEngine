#include "RenderInternal.hpp"

namespace worlds {
    RenderTexture::RenderTexture(VulkanHandles* ctx, RTResourceCreateInfo resourceCreateInfo, const char* debugName) {
        image = vku::GenericImage{ 
            ctx->device, ctx->allocator, 
            resourceCreateInfo.ici, resourceCreateInfo.viewType, 
            (VkImageAspectFlags)resourceCreateInfo.aspectFlags, false, 
            debugName 
        };

        aspectFlags = resourceCreateInfo.aspectFlags;
    }
}
