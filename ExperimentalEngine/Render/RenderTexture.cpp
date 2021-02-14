#include "Render.hpp"

namespace worlds {
    RenderTexture::RenderTexture(const VulkanHandles& ctx, RTResourceCreateInfo resourceCreateInfo, const char* debugName) {
        image = vku::GenericImage{ 
            ctx.device, ctx.allocator, 
            resourceCreateInfo.ici, resourceCreateInfo.viewType, 
            resourceCreateInfo.aspectFlags, false, 
            debugName 
        };

        aspectFlags = resourceCreateInfo.aspectFlags;
    }
}