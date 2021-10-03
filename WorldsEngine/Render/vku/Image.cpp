#include "vku.hpp"

#define UNUSED(x) (void)x
namespace vku {
    GenericImage::GenericImage() {}

    GenericImage::GenericImage(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo& info, VkImageViewType viewType, VkImageAspectFlags aspectMask, bool makeHostImage, const char* debugName) {
        create(device, allocator, info, viewType, aspectMask, makeHostImage, debugName);
    }

    GenericImage::GenericImage(GenericImage&& other) noexcept {
        s = other.s;
        assert(!other.s.destroyed);
        other.s.destroyed = true;
        s.destroyed = false;
    }

    GenericImage& GenericImage::operator=(GenericImage&& other) noexcept {
        s = other.s;
        assert(!other.s.destroyed);
        other.s.destroyed = true;
        s.destroyed = false;
        return *this;
    }

    void GenericImage::clear(VkCommandBuffer cb, const std::array<float, 4> colour) {
        assert(!s.destroyed);
        setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkClearColorValue ccv;
        ccv.float32[0] = colour[0];
        ccv.float32[1] = colour[1];
        ccv.float32[2] = colour[2];
        ccv.float32[3] = colour[3];

        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        vkCmdClearColorImage(cb, s.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ccv, 1, &range);
    }

    void GenericImage::update(VkDevice device, const void* data, VkDeviceSize bytesPerPixel) {
        const uint8_t* src = (const uint8_t*)data;
        for (uint32_t mipLevel = 0; mipLevel != info().mipLevels; ++mipLevel) {
            // Array images are layed out horizontally. eg. [left][front][right] etc.
            for (uint32_t arrayLayer = 0; arrayLayer != info().arrayLayers; ++arrayLayer) {
                VkImageSubresource subresource{ VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, arrayLayer };
                VkSubresourceLayout srLayout;
                vkGetImageSubresourceLayout(device, s.image, &subresource, &srLayout);

                uint8_t* dest;
                vmaMapMemory(s.allocator, s.allocation, (void**)&dest);
                dest += srLayout.offset;
                size_t bytesPerLine = s.info.extent.width * bytesPerPixel;
                size_t srcStride = bytesPerLine * info().arrayLayers;
                for (uint32_t y = 0; y != s.info.extent.height; ++y) {
                    memcpy(dest, src + arrayLayer * bytesPerLine, bytesPerLine);
                    src += srcStride;
                    dest += srLayout.rowPitch;
                }
                vmaUnmapMemory(s.allocator, s.allocation);
            }
        }
    }

    void GenericImage::copy(VkCommandBuffer cb, vku::GenericImage& srcImage) {
        srcImage.setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        for (uint32_t mipLevel = 0; mipLevel != info().mipLevels; ++mipLevel) {
            VkImageCopy region{};
            region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0, 1 };
            region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0, 1 };
            region.extent = s.info.extent;
            vkCmdCopyImage(cb, srcImage.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }
    }

    void GenericImage::copy(VkCommandBuffer cb, VkBuffer buffer, uint32_t mipLevel, uint32_t arrayLayer, uint32_t width, uint32_t height, uint32_t depth, uint32_t offset) {
        setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy region{};
        region.bufferOffset = offset;
        VkExtent3D extent;
        extent.width = width;
        extent.height = height;
        extent.depth = depth;
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, arrayLayer, 1 };
        region.imageExtent = extent;
        vkCmdCopyBufferToImage(cb, buffer, s.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    void GenericImage::upload(VkDevice device, VmaAllocator allocator, std::vector<uint8_t>& bytes, VkCommandPool commandPool, VkPhysicalDeviceMemoryProperties memprops, VkQueue queue, uint32_t numMips) {
        UNUSED(memprops);
        vku::GenericBuffer stagingBuffer(device, allocator, (VkBufferUsageFlags)VK_BUFFER_USAGE_TRANSFER_SRC_BIT, (VkDeviceSize)bytes.size(), VMA_MEMORY_USAGE_CPU_ONLY);
        stagingBuffer.updateLocal(device, (const void*)bytes.data(), bytes.size());

        if (numMips == ~0u)
            numMips = s.info.mipLevels;

        // Copy the staging buffer to the GPU texture and set the layout.
        vku::executeImmediately(device, commandPool, queue, [&](VkCommandBuffer cb) {
            auto bp = getBlockParams(s.info.format);
            VkBuffer buf = stagingBuffer.buffer();
            uint32_t offset = 0;
            for (uint32_t mipLevel = 0; mipLevel != numMips; ++mipLevel) {
                auto width = mipScale(s.info.extent.width, mipLevel);
                auto height = mipScale(s.info.extent.height, mipLevel);
                auto depth = mipScale(s.info.extent.depth, mipLevel);
                for (uint32_t face = 0; face != s.info.arrayLayers; ++face) {
                    copy(cb, buf, mipLevel, face, width, height, depth, offset);
                    offset += ((bp.bytesPerBlock + 3) & ~3) * ((width / bp.blockWidth) * (height / bp.blockHeight));
                }
            }
            setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
            });
    }

    void GenericImage::upload(VkDevice device, VmaAllocator allocator, std::vector<uint8_t>& bytes, VkPhysicalDeviceMemoryProperties memprops, VkCommandBuffer cb, uint32_t numMips) {
        UNUSED(memprops);
        vku::GenericBuffer stagingBuffer(device, allocator, (VkBufferUsageFlags)VK_BUFFER_USAGE_TRANSFER_SRC_BIT, (VkDeviceSize)bytes.size(), VMA_MEMORY_USAGE_CPU_ONLY);
        stagingBuffer.updateLocal(device, (const void*)bytes.data(), bytes.size());

        // Copy the staging buffer to the GPU texture and set the layout.
        {
            if (numMips == ~0u)
                numMips = s.info.mipLevels;
            auto bp = getBlockParams(s.info.format);
            VkBuffer buf = stagingBuffer.buffer();
            uint32_t offset = 0;
            for (uint32_t mipLevel = 0; mipLevel != numMips; ++mipLevel) {
                auto width = mipScale(s.info.extent.width, mipLevel);
                auto height = mipScale(s.info.extent.height, mipLevel);
                auto depth = mipScale(s.info.extent.depth, mipLevel);
                for (uint32_t face = 0; face != s.info.arrayLayers; ++face) {
                    copy(cb, buf, mipLevel, face, width, height, depth, offset);
                    offset += ((bp.bytesPerBlock + 3) & ~3) * ((width / bp.blockWidth) * (height / bp.blockHeight));
                }
            }
            setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }

    void GenericImage::setLayout(VkCommandBuffer cb, VkImageLayout newLayout, VkImageAspectFlags aspectMask) {
        if (newLayout == s.currentLayout || newLayout == VK_IMAGE_LAYOUT_UNDEFINED) return;
        VkImageLayout oldLayout = s.currentLayout;
        s.currentLayout = newLayout;

        VkImageMemoryBarrier imageMemoryBarriers{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarriers.oldLayout = oldLayout;
        imageMemoryBarriers.newLayout = newLayout;
        imageMemoryBarriers.image = s.image;
        imageMemoryBarriers.subresourceRange = { aspectMask, 0, s.info.mipLevels, 0, s.info.arrayLayers };

        // Put barrier on top
        VkPipelineStageFlags srcStageMask{ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
        VkPipelineStageFlags dstStageMask{ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
        VkDependencyFlags dependencyFlags{};
        VkAccessFlags srcMask = s.lastUsageAccessFlags;
        VkAccessFlags dstMask{};

        switch (newLayout) {
        case VK_IMAGE_LAYOUT_UNDEFINED: break;
        case VK_IMAGE_LAYOUT_GENERAL: dstMask = VK_ACCESS_TRANSFER_WRITE_BIT; dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT; break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: dstMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: dstMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: dstMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT; dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: dstMask = VK_ACCESS_SHADER_READ_BIT; dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT; break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: dstMask = VK_ACCESS_TRANSFER_READ_BIT; dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT; break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: dstMask = VK_ACCESS_TRANSFER_WRITE_BIT; dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT; break;
        case VK_IMAGE_LAYOUT_PREINITIALIZED: dstMask = VK_ACCESS_TRANSFER_WRITE_BIT; dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT; break;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: dstMask = VK_ACCESS_MEMORY_READ_BIT; break;
        default: break;
        }
        //printf("%08x %08x\n", (VkFlags)srcMask, (VkFlags)dstMask);

        imageMemoryBarriers.srcAccessMask = srcMask;
        imageMemoryBarriers.dstAccessMask = dstMask;

        s.lastUsageAccessFlags = dstMask;
        s.lastUsageStage = dstStageMask;

        vkCmdPipelineBarrier(cb, srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, 0, nullptr, 1, &imageMemoryBarriers);
    }

    void GenericImage::barrier(VkCommandBuffer& cb, VkPipelineStageFlags fromPS, VkPipelineStageFlags toPS, VkAccessFlags fromAF, VkAccessFlags toAF) {
        VkImageMemoryBarrier imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.srcAccessMask = fromAF;
        imb.dstAccessMask = toAF;
        imb.newLayout = s.currentLayout;
        imb.oldLayout = s.currentLayout;
        imb.subresourceRange = { s.aspectFlags, 0, s.info.mipLevels, 0, s.info.arrayLayers };
        imb.image = s.image;
        s.lastUsageAccessFlags = toAF;
        s.lastUsageStage = toPS;

        vkCmdPipelineBarrier(
            cb,
            fromPS, toPS,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &imb
        );
    }

    void GenericImage::setLayout(VkCommandBuffer cb, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectMask) {
        if (newLayout == s.currentLayout || newLayout == VK_IMAGE_LAYOUT_UNDEFINED) return;
        VkImageLayout oldLayout = s.currentLayout;
        s.currentLayout = newLayout;
        s.lastUsageAccessFlags = dstMask;
        s.lastUsageStage = dstStageMask;

        VkImageMemoryBarrier imageMemoryBarriers{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarriers.oldLayout = oldLayout;
        imageMemoryBarriers.newLayout = newLayout;
        imageMemoryBarriers.image = s.image;
        imageMemoryBarriers.subresourceRange = { aspectMask, 0, s.info.mipLevels, 0, s.info.arrayLayers };

        // Put barrier on top
        imageMemoryBarriers.srcAccessMask = srcMask;
        imageMemoryBarriers.dstAccessMask = dstMask;

        vkCmdPipelineBarrier(cb,
            srcStageMask, dstStageMask,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarriers);
    }

    void* GenericImage::map() {
        assert(!s.destroyed);
        void* res;
        vmaMapMemory(s.allocator, s.allocation, &res);
        return res;
    }

    void GenericImage::unmap() {
        assert(!s.destroyed);
        vmaUnmapMemory(s.allocator, s.allocation);
    }

    void GenericImage::destroy() {
        if (s.destroyed) return;
        if (s.image) {
            if (s.imageView) {
                vkDestroyImageView(s.device, s.imageView, nullptr);
            }

            vkDestroyImage(s.device, s.image, nullptr);

            vmaFreeMemory(s.allocator, s.allocation);
        }
        s.destroyed = true;
    }

    void GenericImage::create(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo& info, VkImageViewType viewType, VkImageAspectFlags aspectMask, bool hostImage, const char* debugName) {
        s.device = device;
        s.allocator = allocator;
        s.currentLayout = info.initialLayout;
        s.info = info;
        s.aspectFlags = aspectMask;
        s.device = device;
        VKCHECK(vkCreateImage(device, &info, nullptr, &s.image));

        if (debugName) {
            VkDebugUtilsObjectNameInfoEXT nameInfo;
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.pObjectName = debugName;
            nameInfo.objectHandle = (uint64_t)s.image;
            nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
            nameInfo.pNext = nullptr;
            auto setObjName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
            setObjName(device, &nameInfo);
        }

        VmaAllocationCreateInfo vaci{};
        vaci.usage = hostImage ? VMA_MEMORY_USAGE_CPU_ONLY : VMA_MEMORY_USAGE_GPU_ONLY;
        vaci.memoryTypeBits = 0;
        vaci.pool = VK_NULL_HANDLE;
        vaci.preferredFlags = 0;
        vaci.requiredFlags = 0;
        vaci.memoryTypeBits = 0;
        vaci.pUserData = (char*)debugName;
        vaci.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        VmaAllocationInfo vci;

        VKCHECK(vmaAllocateMemoryForImage(allocator, s.image, &vaci, &s.allocation, &vci));
        VKCHECK(vkBindImageMemory(device, s.image, vci.deviceMemory, vci.offset));

        const VkImageUsageFlags viewFlags =
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

        if (!hostImage && (uint32_t)(info.usage & viewFlags) != 0) {
            VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            viewInfo.image = s.image;
            viewInfo.viewType = viewType;
            viewInfo.format = info.format;
            viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
            viewInfo.subresourceRange = VkImageSubresourceRange{ aspectMask, 0, info.mipLevels, 0, info.arrayLayers };
            VKCHECK(vkCreateImageView(device, &viewInfo, nullptr, &s.imageView));

            if (debugName) {
                VkDebugUtilsObjectNameInfoEXT nameInfo;
                nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
                nameInfo.pObjectName = debugName;
                nameInfo.objectHandle = (uint64_t)s.imageView;
                nameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
                nameInfo.pNext = nullptr;
                auto setObjName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
                setObjName(device, &nameInfo);
            }
        }
    }

    TextureImage2D::TextureImage2D() {}

    TextureImage2D::TextureImage2D(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, bool hostImage, const char* debugName) {
        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.flags = {};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = VkExtent3D{ width, height, 1U };
        info.mipLevels = mipLevels;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = hostImage ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
        info.usage =
              VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;

        info.initialLayout = hostImage ? VK_IMAGE_LAYOUT_PREINITIALIZED : VK_IMAGE_LAYOUT_UNDEFINED;
        create(device, allocator, info, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, hostImage, debugName);
    }

    TextureImageCube::TextureImageCube() {}

    TextureImageCube::TextureImageCube(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, bool hostImage, const char* debugName, VkImageUsageFlags usageFlags) {
        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.flags = { VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT };
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = VkExtent3D{ width, height, 1U };
        info.mipLevels = mipLevels;
        info.arrayLayers = 6;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = hostImage ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
        info.usage = usageFlags;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;
        info.initialLayout = hostImage ? VK_IMAGE_LAYOUT_PREINITIALIZED : VK_IMAGE_LAYOUT_UNDEFINED;
        create(device, allocator, info, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_ASPECT_COLOR_BIT, hostImage, debugName);
    }

    DepthStencilImage::DepthStencilImage() {}

    DepthStencilImage::DepthStencilImage(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkFormat format, const char* debugName) {
        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.flags = {};

        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = VkExtent3D{ width, height, 1U };
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = samples;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        create(device, allocator, info, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, false, debugName);
    }

    ColorAttachmentImage::ColorAttachmentImage() {}

    ColorAttachmentImage::ColorAttachmentImage(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, VkFormat format, const char* debugName) {
        VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };

        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = VkExtent3D{ width, height, 1U };
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        create(device, allocator, info, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, false, debugName);
    }
}
