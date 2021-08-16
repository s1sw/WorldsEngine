#include "PCH.hpp"
#include "../Core/Engine.hpp"
#include "../Core/Log.hpp"
#include "RenderInternal.hpp"
#include <SDL_messagebox.h>

namespace worlds {
    vk::SurfaceFormatKHR findSurfaceFormat(vk::PhysicalDevice pd, vk::SurfaceKHR surface) {
        auto fmts = pd.getSurfaceFormatsKHR(surface);
        vk::SurfaceFormatKHR fmt;
        fmt = fmts[0];

        if (fmts.size() == 1 && fmt.format == vk::Format::eUndefined) {
            return vk::SurfaceFormatKHR(vk::Format::eB8G8R8A8Unorm);
        } else {
            for (auto& fmt : fmts) {
                if (fmt.format == vk::Format::eB8G8R8A8Unorm) {
                    return fmt;
                }
            }
        }

        return fmt;
    }

    Swapchain::Swapchain(
        vk::PhysicalDevice& physicalDevice,
        vk::Device& device,
        vk::SurfaceKHR& surface,
        QueueFamilyIndices qfi,
        bool fullscreen,
        vk::SwapchainKHR oldSwapchain,
        vk::PresentModeKHR requestedPresentMode)
        : device(device)
        , format(vk::Format::eUndefined) {

        auto surfaceFormat = findSurfaceFormat(physicalDevice, surface);

        auto surfaceCaps = physicalDevice.getSurfaceCapabilitiesKHR(surface);
        this->width = surfaceCaps.currentExtent.width;
        this->height = surfaceCaps.currentExtent.height;

        auto pms = physicalDevice.getSurfacePresentModesKHR(surface);
        vk::PresentModeKHR presentMode = pms[0];
        if (std::find(pms.begin(), pms.end(), requestedPresentMode) != pms.end()) {
            presentMode = requestedPresentMode;
        } else {
            logErr(worlds::WELogCategoryRender, "Failed to find requested present mode");
        }

        vk::SwapchainCreateInfoKHR swapinfo;
        std::array<uint32_t, 2> queueFamilyIndices = { qfi.graphics, qfi.present };
        bool sameQueues = queueFamilyIndices[0] == queueFamilyIndices[1];
        vk::SharingMode sharingMode = !sameQueues ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive;
        swapinfo.imageExtent = surfaceCaps.currentExtent;
        swapinfo.surface = surface;

        uint32_t minImageCount = surfaceCaps.minImageCount;

        if (fullscreen) {
            minImageCount = surfaceCaps.minImageCount < 2 ? 2 : surfaceCaps.minImageCount;
        }

        swapinfo.minImageCount = minImageCount;
        swapinfo.imageFormat = surfaceFormat.format;
        swapinfo.imageColorSpace = surfaceFormat.colorSpace;
        swapinfo.imageExtent = surfaceCaps.currentExtent;
        swapinfo.imageArrayLayers = 1;
        swapinfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;

        swapinfo.imageSharingMode = sharingMode;
        swapinfo.queueFamilyIndexCount = !sameQueues ? 2 : 0;
        swapinfo.pQueueFamilyIndices = queueFamilyIndices.data();
        swapinfo.preTransform = surfaceCaps.currentTransform;
        swapinfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapinfo.presentMode = presentMode;
        swapinfo.clipped = 0;
        swapinfo.oldSwapchain = oldSwapchain;

        auto originalImageUsage = swapinfo.imageUsage;
        swapchain = device.createSwapchainKHRUnique(swapinfo);

        if (swapinfo.imageUsage != originalImageUsage) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning",
                "RTSS detected. If you have issues with crashes or poor performance,\n"
                "please close it as it does not use the Vulkan API properly and can break things.",
                nullptr);
        }

        format = surfaceFormat.format;

        images = device.getSwapchainImagesKHR(*swapchain);
        for (auto& image : images) {
            VkDebugUtilsObjectNameInfoEXT nameInfo;
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.pObjectName = "Swapchain Image";
            nameInfo.objectHandle = (uint64_t)(VkImage)image;
            nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
            nameInfo.pNext = nullptr;

            auto setObjName = (PFN_vkSetDebugUtilsObjectNameEXT)device.getProcAddr("vkSetDebugUtilsObjectNameEXT");
            setObjName(device, &nameInfo);

            vk::ImageViewCreateInfo ivci;
            ivci.image = image;
            ivci.viewType = vk::ImageViewType::e2D;
            ivci.format = format;
            ivci.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
            imageViews.push_back(device.createImageView(ivci));
        }
    }

    Swapchain::~Swapchain() {
        for (auto& iv : imageViews) {
            device.destroyImageView(iv);
        }
    }

    vk::Result Swapchain::acquireImage(vk::Device& device, vk::Semaphore semaphore, uint32_t* imageIndex) {
        return device.acquireNextImageKHR(*swapchain, UINT64_MAX, semaphore, nullptr, imageIndex);
    }
}
