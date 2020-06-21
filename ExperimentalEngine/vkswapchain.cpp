#include "Engine.hpp"

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
	vk::SwapchainKHR oldSwapchain) :
	device(device) {

	auto surfaceFormat = findSurfaceFormat(physicalDevice, surface);

	auto surfaceCaps = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	this->width = surfaceCaps.currentExtent.width;
	this->height = surfaceCaps.currentExtent.height;

	auto pms = physicalDevice.getSurfacePresentModesKHR(surface);
	vk::PresentModeKHR presentMode = pms[0];
	if (std::find(pms.begin(), pms.end(), vk::PresentModeKHR::eFifo) != pms.end()) {
		presentMode = vk::PresentModeKHR::eFifo;
	} else {
		std::cout << "No FIFO mode available\n";
		return;
	}

	vk::SwapchainCreateInfoKHR swapinfo;
	std::array<uint32_t, 2> queueFamilyIndices = { qfi.graphics, qfi.present };
	bool sameQueues = queueFamilyIndices[0] == queueFamilyIndices[1];
	vk::SharingMode sharingMode = !sameQueues ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive;
	swapinfo.imageExtent = surfaceCaps.currentExtent;
	swapinfo.surface = surface;
	swapinfo.minImageCount = surfaceCaps.minImageCount + 1;
	swapinfo.imageFormat = surfaceFormat.format;
	swapinfo.imageColorSpace = surfaceFormat.colorSpace;
	swapinfo.imageExtent = surfaceCaps.currentExtent;
	swapinfo.imageArrayLayers = 1;
	swapinfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;
	swapinfo.imageSharingMode = sharingMode;
	swapinfo.queueFamilyIndexCount = !sameQueues ? 2 : 0;
	swapinfo.pQueueFamilyIndices = queueFamilyIndices.data();
	swapinfo.preTransform = surfaceCaps.currentTransform;;
	swapinfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	swapinfo.presentMode = presentMode;
	swapinfo.clipped = 1;
	swapinfo.oldSwapchain = oldSwapchain;
	swapchain = device.createSwapchainKHRUnique(swapinfo);
	format = surfaceFormat.format;

	images = device.getSwapchainImagesKHR(*swapchain);
	for (auto& image : images) {
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
	return device.acquireNextImageKHR(*swapchain, UINT64_MAX, semaphore, vk::Fence(), imageIndex);
}