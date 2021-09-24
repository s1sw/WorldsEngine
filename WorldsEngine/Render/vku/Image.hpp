#pragma once
#include "vku.hpp"

#define UNUSED(thing) (void)thing
namespace vku {
    /// Generic image with a view and memory object.
    /// Vulkan images need a memory object to hold the data and a view object for the GPU to access the data.
    class GenericImage {
    public:
        GenericImage();

        GenericImage(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo& info, VkImageViewType viewType, VkImageAspectFlags aspectMask, bool makeHostImage, const char* debugName = nullptr);

        GenericImage(GenericImage const&) = delete;

        GenericImage(GenericImage&& other) noexcept;

        GenericImage& operator=(GenericImage&& other) noexcept;

        VkImage image() const { assert(!s.destroyed);  return s.image; }
        VkImageView imageView() const { assert(!s.destroyed);  return s.imageView; }

        /// Clear the colour of an image.
        void clear(VkCommandBuffer cb, const std::array<float, 4> colour = { 1, 1, 1, 1 });

        /// Update the image with an array of pixels. (Currently 2D only)
        void update(VkDevice device, const void* data, VkDeviceSize bytesPerPixel);

        /// Copy another image to this one. This also changes the layout.
        void copy(VkCommandBuffer cb, vku::GenericImage& srcImage);

        /// Copy a subimage in a buffer to this image.
        void copy(VkCommandBuffer cb, VkBuffer buffer, uint32_t mipLevel, uint32_t arrayLayer, uint32_t width, uint32_t height, uint32_t depth, uint32_t offset);

        void upload(VkDevice device, VmaAllocator allocator, std::vector<uint8_t>& bytes, VkCommandPool commandPool, VkPhysicalDeviceMemoryProperties memprops, VkQueue queue, uint32_t numMips = ~0u);

        void upload(VkDevice device, VmaAllocator allocator, std::vector<uint8_t>& bytes, VkPhysicalDeviceMemoryProperties memprops, VkCommandBuffer cb, uint32_t numMips = ~0u);

        /// Change the layout of this image using a memory barrier.
        void setLayout(VkCommandBuffer cb, VkImageLayout newLayout, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

        void barrier(VkCommandBuffer& cb, VkPipelineStageFlags fromPS, VkPipelineStageFlags toPS, VkAccessFlags fromAF, VkAccessFlags toAF);

        void setLayout(VkCommandBuffer cb, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

        void setLayout(VkCommandBuffer cmdBuf, VkImageLayout newLayout, VkPipelineStageFlags newStage, VkAccessFlags newAccess, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT) {
            setLayout(cmdBuf, newLayout, s.lastUsageStage, newStage, s.lastUsageAccessFlags, newAccess, aspectMask);
        }

        /// Set what the image thinks is its current layout (ie. the old layout in an image barrier).
        void setCurrentLayout(VkImageLayout oldLayout, VkPipelineStageFlags lastPipelineStage, VkAccessFlags lastAccess) {
            s.currentLayout = oldLayout;
            s.lastUsageAccessFlags = lastAccess;
            s.lastUsageStage = lastPipelineStage;
        }

        VkFormat format() const { return s.info.format; }
        VkExtent3D extent() const { return s.info.extent; }
        const VkImageCreateInfo& info() const { return s.info; }
        VkImageLayout layout() const { return s.currentLayout; }

        void* map();
        void unmap();

        void destroy();

        ~GenericImage() {
            destroy();
        }
    protected:
        void create(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo& info, VkImageViewType viewType, VkImageAspectFlags aspectMask, bool hostImage, const char* debugName);

        struct State {
            VkImage image;
            VkImageView imageView = VK_NULL_HANDLE;
            VkDevice device = VK_NULL_HANDLE;
            VkDeviceSize size = 0;
            VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkPipelineStageFlags lastUsageStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkAccessFlags lastUsageAccessFlags = VK_ACCESS_MEMORY_WRITE_BIT;
            VkImageAspectFlags aspectFlags;
            VkImageCreateInfo info;
            VmaAllocation allocation = nullptr;
            VmaAllocator allocator;
            bool destroyed = false;
        };

        State s;
    };


    /// A 2D texture image living on the GPU or a staging buffer visible to the CPU.
    class TextureImage2D : public GenericImage {
    public:
        TextureImage2D() {
        }

        TextureImage2D(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, uint32_t mipLevels = 1, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, bool hostImage = false, const char* debugName = nullptr) {
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
    private:
    };

    /// A cube map texture image living on the GPU or a staging buffer visible to the CPU.
    class TextureImageCube : public GenericImage {
    public:
        TextureImageCube() {
        }

        TextureImageCube(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, uint32_t mipLevels = 1, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, bool hostImage = false, const char* debugName = nullptr, VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT) {
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
    private:
    };

    /// An image to use as a depth buffer on a renderpass.
    class DepthStencilImage : public GenericImage {
    public:
        DepthStencilImage() {
        }

        DepthStencilImage(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, VkFormat format = VK_FORMAT_D24_UNORM_S8_UINT, const char* debugName = nullptr) {
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
    private:
    };

    /// An image to use as a colour buffer on a renderpass.
    class ColorAttachmentImage : public GenericImage {
    public:
        ColorAttachmentImage() {
        }

        ColorAttachmentImage(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, const char* debugName = nullptr) {
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
    private:
    };
}
#undef UNUSED
