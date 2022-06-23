#include <R2/VKTexture.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <assert.h>

namespace R2::VK
{
    VkImageType convertType(TextureDimension dim)
    {
        switch (dim)
        {
        // "Cube" textures are just a special case of 2D textures
        // in Vulkan
        case TextureDimension::Cube:
        case TextureDimension::ArrayCube:
        case TextureDimension::Array2D:
        case TextureDimension::Dim2D:
        default:
            return VK_IMAGE_TYPE_2D;
        case TextureDimension::Dim3D:
            return VK_IMAGE_TYPE_3D;
        }
    }

    VkImageViewType convertViewType(TextureDimension dim)
    {
        switch (dim)
        {
        case TextureDimension::Cube:
            return VK_IMAGE_VIEW_TYPE_CUBE;
        case TextureDimension::ArrayCube:
            return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        case TextureDimension::Dim2D:
        default:
            return VK_IMAGE_VIEW_TYPE_2D;
        case TextureDimension::Array2D:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureDimension::Dim3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        }
    }

    bool supportsStorage(TextureFormat format)
    {
        return format != TextureFormat::R8G8B8A8_SRGB;
    }

    bool isDimensionCube(TextureDimension dim)
    {
        return dim == TextureDimension::Cube || dim == TextureDimension::ArrayCube;
    }

    bool isFormatDepth(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::D32_SFLOAT:
            return true;
        }

        return false;
    }

    Texture::Texture(const Handles* handles, const TextureCreateInfo& createInfo) : handles(handles)
    {
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.extent.width = createInfo.Width;
        ici.extent.height = createInfo.Height;
        ici.extent.depth = createInfo.Depth;
        ici.arrayLayers = createInfo.Layers;
        ici.mipLevels = createInfo.NumMips;
        ici.samples = static_cast<VkSampleCountFlagBits>(createInfo.Samples);

        if (isDimensionCube(createInfo.Dimension))
        {
            ici.arrayLayers *= 6;
            ici.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        ici.imageType = convertType(createInfo.Dimension);
        ici.format = static_cast<VkFormat>(createInfo.Format);
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        
        if (supportsStorage(createInfo.Format))
            ici.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

        if (createInfo.IsRenderTarget)
        {
            if (isFormatDepth(createInfo.Format))
            {
                ici.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
            else
            {
                ici.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            }
        }

        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vaci{};
        vaci.usage = VMA_MEMORY_USAGE_AUTO;
        VKCHECK(vmaCreateImage(handles->Allocator, &ici, &vaci, &image, &allocation, nullptr));

        // Now copy everything...
        width = createInfo.Width;
        height = createInfo.Height;
        depth = createInfo.Depth;
        layers = createInfo.Layers;
        numMips = createInfo.NumMips;
        format = createInfo.Format;
        dimension = createInfo.Dimension;
        samples = createInfo.Samples;

        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image = image;
        ivci.viewType = convertViewType(createInfo.Dimension);
        ivci.format = ici.format;
        ivci.subresourceRange.aspectMask = getAspectFlags();
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.layerCount = layers;
        ivci.subresourceRange.levelCount = numMips;

        VKCHECK(vkCreateImageView(handles->Device, &ivci, handles->AllocCallbacks, &imageView));
    }

    Texture::Texture(const Handles* handles, VkImage image, const TextureCreateInfo& createInfo)
        : image(image)
        , allocation(nullptr)
    {
        // Now copy everything...
        width = createInfo.Width;
        height = createInfo.Height;
        depth = createInfo.Depth;
        layers = createInfo.Layers;
        numMips = createInfo.NumMips;
        format = createInfo.Format;
        dimension = createInfo.Dimension;
        samples = createInfo.Samples;

        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image = image;
        ivci.viewType = convertViewType(createInfo.Dimension);
        ivci.format = static_cast<VkFormat>(createInfo.Format);
        ivci.subresourceRange.aspectMask = getAspectFlags();
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.layerCount = layers;
        ivci.subresourceRange.levelCount = numMips;

        VKCHECK(vkCreateImageView(handles->Device, &ivci, handles->AllocCallbacks, &imageView));
        this->handles = handles;
    }

    VkImage Texture::GetNativeHandle()
    {
        return image;
    }

    VkImage Texture::ReleaseHandle()
    {
        assert(allocation == nullptr);
        vkDestroyImageView(handles->Device, imageView, handles->AllocCallbacks);
        VkImage tmp = image;
        image = VK_NULL_HANDLE;
        return tmp;
    }

    VkImageView Texture::GetView()
    {
        return imageView;
    }

    int Texture::GetWidth()
    {
        return width;
    }

    int Texture::GetHeight()
    {
        return height;
    }

    int Texture::GetLayers()
    {
        return layers;
    }

    TextureFormat Texture::GetFormat()
    {
        return format;
    }

    void Texture::WriteLayoutTransition(CommandBuffer cb, ImageLayout layout)
    {
        VkImageMemoryBarrier2 imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        imb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imb.newLayout = (VkImageLayout)layout;
        imb.subresourceRange = VkImageSubresourceRange{ getAspectFlags(), 0, (uint32_t)numMips, 0, (uint32_t)layers };
        imb.image = image;
        imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        imb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        
        VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imb;
        depInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        vkCmdPipelineBarrier2(cb.GetNativeHandle(), &depInfo);
    }

    void Texture::WriteLayoutTransition(CommandBuffer cb, ImageLayout oldLayout, ImageLayout newLayout)
    {
        VkImageMemoryBarrier2 imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        imb.oldLayout = (VkImageLayout)oldLayout;
        imb.newLayout = (VkImageLayout)newLayout;
        imb.subresourceRange = VkImageSubresourceRange{ getAspectFlags(), 0, (uint32_t)numMips, 0, (uint32_t)layers };
        imb.image = image;
        imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        imb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;

        VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imb;
        depInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        vkCmdPipelineBarrier2(cb.GetNativeHandle(), &depInfo);
    }

    Texture::~Texture()
    {
        if (allocation)
        {
            vmaDestroyImage(handles->Allocator, image, allocation);
            vkDestroyImageView(handles->Device, imageView, handles->AllocCallbacks);
        }
        else if (image)
        {
            vkDestroyImage(handles->Device, image, handles->AllocCallbacks);
            vkDestroyImageView(handles->Device, imageView, handles->AllocCallbacks);
        }
    }

    VkImageAspectFlags Texture::getAspectFlags() const
    {
        if (format == TextureFormat::D32_SFLOAT)
        {
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}