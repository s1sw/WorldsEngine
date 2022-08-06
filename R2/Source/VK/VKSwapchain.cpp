#include <R2/VKSwapchain.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKEnums.hpp>
#include <R2/VKSyncPrims.hpp>
#include <R2/VKTexture.hpp>
#include <R2/VKUtil.hpp>
#include <volk.h>
#include <vector>

namespace R2::VK
{
    bool isFormatCached = false;
    VkSurfaceFormatKHR cachedFormat;

    VkSurfaceFormatKHR findSurfaceFormat(VkPhysicalDevice pd, VkSurfaceKHR surface)
    {
        if (isFormatCached)
            return cachedFormat;

        uint32_t fmtCount;
        VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &fmtCount, nullptr));

        std::vector<VkSurfaceFormatKHR> fmts;
        fmts.resize(fmtCount);

        VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &fmtCount, fmts.data()));
        VkSurfaceFormatKHR fmt;
        fmt = fmts[0];

        if (fmts.size() == 1 && fmt.format == VK_FORMAT_UNDEFINED)
        {
            return VkSurfaceFormatKHR{
                VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            };
        }
        else
        {
            for (auto& fmt : fmts)
            {
                if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB)
                {
                    isFormatCached = true;
                    cachedFormat = fmt;
                    return fmt;
                }
            }
        }

        isFormatCached = true;
        cachedFormat = fmt;
        return fmt;
    }

    Swapchain::Swapchain(Core* renderer, const SwapchainCreateInfo& createInfo)
        : handles(renderer->GetHandles())
        , renderer(renderer)
        , swapchain(VK_NULL_HANDLE)
    {
        surface = createInfo.surface;
        vsyncEnabled = true;

        recreate();
    }

    Swapchain::~Swapchain()
    {
        destroySwapchain();
    }

    void Swapchain::SetVsync(bool vsync)
    {
        vsyncEnabled = vsync;
        recreate();
    }

    bool Swapchain::GetVsync() const
    {
        return vsyncEnabled;
    }

    void Swapchain::Present()
    {
        VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.pSwapchains = &swapchain;
        presentInfo.swapchainCount = 1;
        presentInfo.pImageIndices = &acquiredImageIndex;
        VkSemaphore completionSemaphore = renderer->GetFrameCompletionSemaphore();
        presentInfo.pWaitSemaphores = &completionSemaphore;
        presentInfo.waitSemaphoreCount = 1;

        VkResult result = vkQueuePresentKHR(handles->Queues.Graphics, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            recreate();
        }
    }

    void Swapchain::Resize()
    {
        recreate();
    }

    void Swapchain::Resize(int width, int height)
    {
        recreate(width, height);
    }

    void Swapchain::GetSize(int& width, int& height)
    {
        width = this->width;
        height = this->height;
    }

    Texture* Swapchain::Acquire(Fence* fence)
    {
        uint32_t imageIndex;
        vkAcquireNextImageKHR(handles->Device, swapchain, UINT64_MAX, VK_NULL_HANDLE, fence->GetNativeHandle(), &imageIndex);
        acquiredImageIndex = imageIndex;

        return imageTextures[imageIndex];
    }

    void Swapchain::recreate()
    {
        VkSurfaceCapabilitiesKHR surfaceCaps;
        VKCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(handles->PhysicalDevice, surface, &surfaceCaps));
        recreate(surfaceCaps.currentExtent.width, surfaceCaps.currentExtent.height);
    }

    void Swapchain::recreate(int width, int height)
    {
        VkSurfaceCapabilitiesKHR surfaceCaps;
        VKCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(handles->PhysicalDevice, surface, &surfaceCaps));

        VkSurfaceFormatKHR format = findSurfaceFormat(handles->PhysicalDevice, surface);

        VkSwapchainCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.imageExtent.width = width;
        createInfo.imageExtent.height = height;
        createInfo.minImageCount = surfaceCaps.minImageCount;

        createInfo.imageFormat = format.format;
        createInfo.imageColorSpace = format.colorSpace;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        createInfo.queueFamilyIndexCount = 0;
        createInfo.preTransform = surfaceCaps.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = vsyncEnabled ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        createInfo.clipped = VK_FALSE;
        createInfo.surface = surface;

        if (swapchain)
        {
            createInfo.oldSwapchain = swapchain;
        }

        VkSwapchainKHR oldSwapchain = swapchain;

        VKCHECK(vkCreateSwapchainKHR(handles->Device, &createInfo, handles->AllocCallbacks, &swapchain));

        if (oldSwapchain)
        {
            for (Texture* tex : imageTextures)
            {
                tex->ReleaseHandle();
            }

            imageTextures.clear();
            images.clear();

            vkDestroySwapchainKHR(handles->Device, oldSwapchain, handles->AllocCallbacks);
        }

        uint32_t swapchainImageCount;
        VKCHECK(vkGetSwapchainImagesKHR(handles->Device, swapchain, &swapchainImageCount, nullptr));

        images.resize(swapchainImageCount);

        VKCHECK(vkGetSwapchainImagesKHR(handles->Device, swapchain, &swapchainImageCount, images.data()));

        TextureCreateInfo swapTexInfo = TextureCreateInfo::Texture2D(
            static_cast<TextureFormat>(format.format), createInfo.imageExtent.width, createInfo.imageExtent.height);

        for (VkImage image : images)
        {
            imageTextures.push_back(new Texture(renderer, image, ImageLayout::Undefined, swapTexInfo));
        }

        this->width = createInfo.imageExtent.width;
        this->height = createInfo.imageExtent.height;

        VkCommandBuffer cb = Utils::AcquireImmediateCommandBuffer();
        for (Texture* tex : imageTextures)
        {
            tex->Acquire(cb, ImageLayout::PresentSrc, AccessFlags::MemoryRead);
        }
        Utils::ExecuteImmediateCommandBuffer();
    }

    void Swapchain::destroySwapchain()
    {
        for (Texture* tex : imageTextures)
        {
            tex->ReleaseHandle();
        }

        imageTextures.clear();
        images.clear();

        vkDestroySwapchainKHR(handles->Device, swapchain, handles->AllocCallbacks);
    }
}