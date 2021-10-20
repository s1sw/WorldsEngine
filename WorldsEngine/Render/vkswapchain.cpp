#include "../Core/Engine.hpp"
#include "../Core/Log.hpp"
#include "RenderInternal.hpp"
#include <SDL_messagebox.h>

namespace worlds {
    VkSurfaceFormatKHR findSurfaceFormat(VkPhysicalDevice pd, VkSurfaceKHR surface) {
        uint32_t fmtCount;
        VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &fmtCount, nullptr));

        std::vector<VkSurfaceFormatKHR> fmts;
        fmts.resize(fmtCount);

        VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &fmtCount, fmts.data()));
        VkSurfaceFormatKHR fmt;
        fmt = fmts[0];

        if (fmts.size() == 1 && fmt.format == VK_FORMAT_UNDEFINED) {
            return VkSurfaceFormatKHR {
                VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            };
        } else {
            for (auto& fmt : fmts) {
                if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM) {
                    return fmt;
                }
            }
        }

        return fmt;
    }

    Swapchain::Swapchain(
        VkPhysicalDevice& physicalDevice,
        VkDevice device,
        VkSurfaceKHR& surface,
        QueueFamilies qfi,
        bool fullscreen,
        VkSwapchainKHR oldSwapchain,
        VkPresentModeKHR requestedPresentMode)
        : device(device)
        , format(VK_FORMAT_UNDEFINED) {

        auto surfaceFormat = findSurfaceFormat(physicalDevice, surface);

        VkSurfaceCapabilitiesKHR surfCaps;
        VKCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps));

        this->width = surfCaps.currentExtent.width;
        this->height = surfCaps.currentExtent.height;

        uint32_t presentModeCount;
        VKCHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));

        std::vector<VkPresentModeKHR> pms(presentModeCount);
        VKCHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, pms.data()));

        VkPresentModeKHR presentMode = pms[0];
        if (std::find(pms.begin(), pms.end(), requestedPresentMode) != pms.end()) {
            presentMode = requestedPresentMode;
        } else {
            logErr(worlds::WELogCategoryRender, "Failed to find requested present mode");
        }

        VkSwapchainCreateInfoKHR swapinfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        std::array<uint32_t, 2> queueFamilyIndices = { qfi.graphics, qfi.present };
        bool sameQueues = queueFamilyIndices[0] == queueFamilyIndices[1];
        VkSharingMode sharingMode = !sameQueues ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        swapinfo.imageExtent = surfCaps.currentExtent;
        swapinfo.surface = surface;

        uint32_t minImageCount = surfCaps.minImageCount;

        if (fullscreen) {
            minImageCount = surfCaps.minImageCount < 2 ? 2 : surfCaps.minImageCount;
        }

        swapinfo.minImageCount = minImageCount;
        swapinfo.imageFormat = surfaceFormat.format;
        swapinfo.imageColorSpace = surfaceFormat.colorSpace;
        swapinfo.imageExtent = surfCaps.currentExtent;
        swapinfo.imageArrayLayers = 1;
        swapinfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        swapinfo.imageSharingMode = sharingMode;
        swapinfo.queueFamilyIndexCount = !sameQueues ? 2 : 0;
        swapinfo.pQueueFamilyIndices = queueFamilyIndices.data();
        swapinfo.preTransform = surfCaps.currentTransform;
        swapinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapinfo.presentMode = presentMode;
        swapinfo.clipped = 0;
        swapinfo.oldSwapchain = oldSwapchain;

        auto originalImageUsage = swapinfo.imageUsage;

        VKCHECK(vkCreateSwapchainKHR(device, &swapinfo, nullptr, &swapchain));

        if (swapinfo.imageUsage != originalImageUsage) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning",
                "RTSS detected. If you have issues with crashes or poor performance,\n"
                "please close it as it does not use the Vulkan API properly and could break things.",
                nullptr);
        }

        format = surfaceFormat.format;

        uint32_t swapchainImageCount;
        VKCHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr));

        images.resize(swapchainImageCount);
        VKCHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, images.data()));

        imageViews.resize(swapchainImageCount);

        int i = 0;
        for (VkImage image : images) {
            VkDebugUtilsObjectNameInfoEXT nameInfo{};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.pObjectName = "Swapchain Image";
            nameInfo.objectHandle = (uint64_t)image;
            nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
            nameInfo.pNext = nullptr;

            vkSetDebugUtilsObjectNameEXT(device, &nameInfo);

            VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            ivci.image = image;
            ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivci.format = format;
            ivci.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            vkCreateImageView(device, &ivci, nullptr, &imageViews[i]);
            i++;
        }
    }

    Swapchain::~Swapchain() {
        for (VkImageView iv : imageViews) {
            vkDestroyImageView(device, iv, nullptr);
        }
        vkDestroySwapchainKHR(device, swapchain, nullptr);
    }

    VkResult Swapchain::acquireImage(VkDevice device, VkSemaphore semaphore, uint32_t* imageIndex) {
        return vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, imageIndex);
    }
}
