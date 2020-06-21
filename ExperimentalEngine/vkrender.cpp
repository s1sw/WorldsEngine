#include "Engine.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "tiny_obj_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "imgui_impl_vulkan.h"
#include "physfs.hpp"
#include "Transform.hpp"

struct StandardPushConstants {
	glm::vec4 pack0;
	glm::mat4 model;
};

struct SDFUniforms {
	glm::vec4 resolution;
};

struct SDFPushConstants {
	glm::vec4 camPos;
	glm::vec4 camRot;
};

uint32_t findPresentQueue(vk::PhysicalDevice pd, vk::SurfaceKHR surface) {
	auto qprops = pd.getQueueFamilyProperties();
	for (uint32_t qi = 0; qi != qprops.size(); ++qi) {
		auto& qprop = qprops[qi];
		if (pd.getSurfaceSupportKHR(qi, surface) && (qprop.queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics) {
			return qi;
		}
	}
	return ~0u;
}

void VKRenderer::createSwapchain(vk::SwapchainKHR oldSwapchain) {
	QueueFamilyIndices qfi{ graphicsQueueFamilyIdx, presentQueueFamilyIdx };
	swapchain = std::make_unique<Swapchain>(physicalDevice, *device, surface, qfi, oldSwapchain);
	swapchain->getSize(&width, &height);

	vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [this](vk::CommandBuffer cb) {
		for (auto& img : swapchain->images)
			vku::transitionLayout(cb, img, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::AccessFlags{}, vk::AccessFlagBits::eMemoryRead);
	});
}

void VKRenderer::createFramebuffers() {
	for (int i = 0; i != swapchain->imageViews.size(); i++) {
		vk::ImageView attachments[2] = { swapchain->imageViews[i], this->depthStencilImage.imageView() };
		vk::FramebufferCreateInfo fci;
		fci.attachmentCount = 2;
		fci.pAttachments = attachments;
		fci.width = this->width;
		fci.height = this->height;
		fci.renderPass = *this->imguiRenderPass;
		fci.layers = 1;
		this->framebuffers.push_back(this->device->createFramebufferUnique(fci));
	}
}

void loadMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::istream& stream) {
	indices.clear();
	vertices.clear();
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn;
	std::string err;

	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &stream);

	// Load the first shape
	size_t index_offset = 0;
	for (auto& idx : shapes[0].mesh.indices) {
		Vertex vert;
		vert.position = glm::vec3(attrib.vertices[3 * (size_t)idx.vertex_index], attrib.vertices[3 * (size_t)idx.vertex_index + 1], attrib.vertices[3 * (size_t)idx.vertex_index + 2]);
		vert.normal = glm::vec3(attrib.normals[3 * (size_t)idx.normal_index], attrib.normals[3 * (size_t)idx.normal_index + 1], attrib.normals[3 * (size_t)idx.normal_index + 2]);
		if (idx.texcoord_index >= 0)
			vert.uv = glm::vec2(attrib.texcoords[2 * (size_t)idx.texcoord_index], attrib.texcoords[2 * (size_t)idx.texcoord_index + 1]);
		vertices.push_back(vert);
		indices.push_back(indices.size());
	}
}

void VKRenderer::loadAlbedo() {
	auto memProps = physicalDevice.getMemoryProperties();
	int x, y, channelsInFile;
	stbi_uc* dat = stbi_load("albedo.png", &x, &y, &channelsInFile, 4);
	albedoTex = vku::TextureImage2D{ *device, memProps, (uint32_t)x, (uint32_t)y, 1, vk::Format::eR8G8B8A8Srgb };

	std::vector<uint8_t> albedoDat(dat, dat + (x * y * 4));

	albedoTex.upload(*device, albedoDat, *commandPool, memProps, device->getQueue(graphicsQueueFamilyIdx, 0));
}

void VKRenderer::setupTonemapping() {
	vku::DescriptorSetLayoutMaker tonemapDslm;
	tonemapDslm.image(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
	tonemapDslm.image(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);

	tonemapDsl = tonemapDslm.createUnique(*device);

	tonemapShader = vku::ShaderModule{ *device, "tonemap.comp.spv" };

	vku::PipelineLayoutMaker plm;
	plm.descriptorSetLayout(*tonemapDsl);

	tonemapPipelineLayout = plm.createUnique(*device);

	vku::ComputePipelineMaker cpm;
	cpm.shader(vk::ShaderStageFlagBits::eCompute, tonemapShader);
	tonemapPipeline = cpm.createUnique(*device, *pipelineCache, *tonemapPipelineLayout);

	vku::DescriptorSetMaker dsm;
	dsm.layout(*tonemapDsl);
	tonemapDescriptorSet = dsm.create(*device, *descriptorPool)[0];
}

void VKRenderer::setupImGUI() {
	vku::RenderpassMaker rPassMaker{};

	rPassMaker.attachmentBegin(vk::Format::eR8G8B8A8Unorm);
	rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eDontCare);
	rPassMaker.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
	rPassMaker.attachmentFinalLayout(vk::ImageLayout::eTransferSrcOptimal);
	rPassMaker.attachmentInitialLayout(vk::ImageLayout::eGeneral);

	rPassMaker.subpassBegin(vk::PipelineBindPoint::eGraphics);
	rPassMaker.subpassColorAttachment(vk::ImageLayout::eGeneral, 0);

	rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
	rPassMaker.dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	rPassMaker.dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	rPassMaker.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

	imguiRenderPass = rPassMaker.createUnique(*device);

	ImGui_ImplVulkan_InitInfo imguiInit;
	memset(&imguiInit, 0, sizeof(imguiInit));
	imguiInit.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	imguiInit.Device = *device;
	imguiInit.Instance = *instance;
	imguiInit.DescriptorPool = *descriptorPool;
	imguiInit.PhysicalDevice = physicalDevice;
	imguiInit.PipelineCache = *pipelineCache;
	imguiInit.Queue = device->getQueue(graphicsQueueFamilyIdx, 0);
	imguiInit.QueueFamily = graphicsQueueFamilyIdx;
	imguiInit.MinImageCount = swapchain->images.size();
	imguiInit.ImageCount = swapchain->images.size();
	ImGui_ImplVulkan_Init(&imguiInit, *imguiRenderPass);

	vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [](vk::CommandBuffer cb) {
		ImGui_ImplVulkan_CreateFontsTexture(cb);
	});
}

void VKRenderer::setupStandard() {
	auto memoryProps = physicalDevice.getMemoryProperties();

	vku::SamplerMaker sm{};
	sm.magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear).mipmapMode(vk::SamplerMipmapMode::eLinear);
	albedoSampler = sm.createUnique(*device);
	loadAlbedo();

	vku::DescriptorSetLayoutMaker dslm;
	dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
	dslm.buffer(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1);
	dslm.buffer(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1);
	dslm.image(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);

	this->dsl = dslm.createUnique(*this->device);

	vku::PipelineLayoutMaker plm;
	plm.pushConstantRange(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, sizeof(StandardPushConstants));
	plm.descriptorSetLayout(*this->dsl);
	this->pipelineLayout = plm.createUnique(*this->device);

	this->vpUB = vku::UniformBuffer(*this->device, memoryProps, sizeof(MVP));
	lightsUB = vku::UniformBuffer(*this->device, memoryProps, sizeof(LightUB));
	materialUB = vku::UniformBuffer(*this->device, memoryProps, sizeof(PackedMaterial));

	PackedMaterial pm{ glm::vec4(0.0f, 10.0f, 0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f) };
	materialUB.upload(*device, memoryProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), pm);

	vku::DescriptorSetMaker dsm;
	dsm.layout(*this->dsl);
	this->descriptorSets = dsm.create(*this->device, *this->descriptorPool);

	vku::DescriptorSetUpdater updater;
	updater.beginDescriptorSet(this->descriptorSets[0]);
	updater.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
	updater.buffer(this->vpUB.buffer(), 0, sizeof(VP));
	updater.beginBuffers(1, 0, vk::DescriptorType::eUniformBuffer);
	updater.buffer(lightsUB.buffer(), 0, sizeof(LightUB));
	updater.beginBuffers(2, 0, vk::DescriptorType::eUniformBuffer);
	updater.buffer(materialUB.buffer(), 0, sizeof(PackedMaterial));
	updater.beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler);
	updater.image(*albedoSampler, albedoTex.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
	updater.update(*this->device);

	vku::RenderpassMaker rPassMaker;

	rPassMaker.attachmentBegin(vk::Format::eR16G16B16A16Sfloat);
	rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
	rPassMaker.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
	rPassMaker.attachmentFinalLayout(vk::ImageLayout::eGeneral);

	rPassMaker.attachmentBegin(vk::Format::eD32Sfloat);
	rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
	rPassMaker.attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
	rPassMaker.attachmentFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

	rPassMaker.subpassBegin(vk::PipelineBindPoint::eGraphics);
	rPassMaker.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);
	rPassMaker.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);

	rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
	rPassMaker.dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	rPassMaker.dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	rPassMaker.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

	this->renderPass = rPassMaker.createUnique(*this->device);

	vertexShader = vku::ShaderModule{ *this->device, "test.vert.spv" };
	fragmentShader = vku::ShaderModule{ *this->device, "test.frag.spv" };
}

VKRenderer::VKRenderer(SDL_Window* window, bool* success) : window(window) {
	vku::InstanceMaker instanceMaker;
	//instanceMaker.defaultLayers();
	unsigned int extCount;
	SDL_Vulkan_GetInstanceExtensions(window, &extCount, nullptr);

	std::vector<const char*> names(extCount);
	SDL_Vulkan_GetInstanceExtensions(window, &extCount, names.data());

	for (auto extName : names)
		instanceMaker.extension(extName);

#ifndef NDEBUG
	instanceMaker.layer("VK_LAYER_KHRONOS_validation");
	instanceMaker.extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

	instanceMaker.applicationName("Experimental Game")
		.engineName("Experimental Engine")
		.applicationVersion(1)
		.engineVersion(1);

	this->instance = instanceMaker.createUnique();
#ifndef NDEBUG
	this->dbgCallback = vku::DebugCallback(*this->instance);
#endif
	auto physDevs = this->instance->enumeratePhysicalDevices();
	// TODO: Go through physical devices and select one properly
	this->physicalDevice = physDevs[0];

	auto memoryProps = this->physicalDevice.getMemoryProperties();

	auto physDevProps = physicalDevice.getProperties();
	std::cout
		<< "Physical device:\n"
		<< "\t-Name: " << physDevProps.deviceName << "\n"
		<< "\t-ID: " << physDevProps.deviceID << "\n"
		<< "\t-Vendor ID: " << physDevProps.vendorID << "\n"
		<< "\t-Device Type: " << vk::to_string(physDevProps.deviceType) << "\n"
		<< "\t-Driver Version: " << physDevProps.driverVersion << "\n"
		<< "\t-Memory heap count: " << memoryProps.memoryHeapCount << "\n"
		<< "\t-Memory type count: " << memoryProps.memoryTypeCount << "\n";

	vk::DeviceSize totalVram = 0;
	for (int i = 0; i < memoryProps.memoryHeapCount; i++) {
		auto& heap = memoryProps.memoryHeaps[i];
		totalVram += heap.size;
		std::cout << "Heap " << i << ": " << heap.size / 1024 / 1024 << "MB\n";
	}

	for (int i = 0; i < memoryProps.memoryTypeCount; i++) {
		auto& memType = memoryProps.memoryTypes[i];
		std::cout << "Memory type for heap " << memType.heapIndex << ": " << vk::to_string(memType.propertyFlags) << "\n";
	}

	std::cout << "Approx. " << totalVram / 1024 / 1024 << "MB total accessible graphics memory (NOT VRAM!)\n";

	auto qprops = this->physicalDevice.getQueueFamilyProperties();
	const auto badQueue = ~(uint32_t)0;
	graphicsQueueFamilyIdx = badQueue;
	computeQueueFamilyIdx = badQueue;
	vk::QueueFlags search = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;

	for (auto& qprop : qprops) {
		std::cout << "Queue with properties " << vk::to_string(qprop.queueFlags) << "\n";
	}

	// Look for an omnipurpose queue family first
	// It is better if we can schedule operations without barriers and semaphores.
	// The Spec says: "If an implementation exposes any queue family that supports graphics operations,
	// at least one queue family of at least one physical device exposed by the implementation
	// must support both graphics and compute operations."
	// Also: All commands that are allowed on a queue that supports transfer operations are
	// also allowed on a queue that supports either graphics or compute operations...
	// As a result we can expect a queue family with at least all three and maybe all four modes.
	for (uint32_t qi = 0; qi != qprops.size(); ++qi) {
		auto& qprop = qprops[qi];
		if ((qprop.queueFlags & search) == search) {
			this->graphicsQueueFamilyIdx = qi;
			this->computeQueueFamilyIdx = qi;
			break;
		}
	}

	// Search for async compute queue family
	asyncComputeQueueFamilyIdx = badQueue;
	for (int i = 0; i < qprops.size(); i++) {
		auto& qprop = qprops[i];
		if ((qprop.queueFlags & (vk::QueueFlagBits::eCompute)) == vk::QueueFlagBits::eCompute && i != computeQueueFamilyIdx) {
			asyncComputeQueueFamilyIdx = i;
			break;
		}
	}

	if (asyncComputeQueueFamilyIdx == badQueue)
		std::cout << "Couldn't find async compute queue\n";

	if (this->graphicsQueueFamilyIdx == badQueue || this->computeQueueFamilyIdx == badQueue) {
		*success = false;
		return;
	}

	vku::DeviceMaker dm{};
	dm.defaultLayers();
	dm.queue(this->graphicsQueueFamilyIdx);
	if (this->computeQueueFamilyIdx != this->graphicsQueueFamilyIdx) dm.queue(this->computeQueueFamilyIdx);
	this->device = dm.createUnique(this->physicalDevice);

	vk::PipelineCacheCreateInfo pipelineCacheInfo{};
	this->pipelineCache = this->device->createPipelineCacheUnique(pipelineCacheInfo);

	std::vector<vk::DescriptorPoolSize> poolSizes;
	poolSizes.emplace_back(vk::DescriptorType::eUniformBuffer, 128);
	poolSizes.emplace_back(vk::DescriptorType::eCombinedImageSampler, 128);
	poolSizes.emplace_back(vk::DescriptorType::eStorageBuffer, 128);

	// Create an arbitrary number of descriptors in a pool.
	// Allow the descriptors to be freed, possibly not optimal behaviour.
	vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
	descriptorPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	descriptorPoolInfo.maxSets = 256;
	descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	descriptorPoolInfo.pPoolSizes = poolSizes.data();
	this->descriptorPool = this->device->createDescriptorPoolUnique(descriptorPoolInfo);

	VkSurfaceKHR surface;
	SDL_Vulkan_CreateSurface(window, *this->instance, &surface);

	this->surface = surface;
	this->presentQueueFamilyIdx = findPresentQueue(this->physicalDevice, this->surface);

	vk::SemaphoreCreateInfo sci;
	this->imageAcquire = this->device->createSemaphoreUnique(sci);
	this->commandComplete = this->device->createSemaphoreUnique(sci);

	vk::CommandPoolCreateInfo cpci;
	cpci.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	cpci.queueFamilyIndex = this->graphicsQueueFamilyIdx;
	this->commandPool = this->device->createCommandPoolUnique(cpci);

	createSwapchain(vk::SwapchainKHR{});

	setupTonemapping();
	setupImGUI();
	setupStandard();

	createSCDependents();

	vk::CommandBufferAllocateInfo cbai;
	cbai.commandPool = *this->commandPool;
	cbai.commandBufferCount = 4;
	cbai.level = vk::CommandBufferLevel::ePrimary;
	this->cmdBufs = this->device->allocateCommandBuffersUnique(cbai);

	for (int i = 0; i < this->cmdBufs.size(); i++) {
		vk::FenceCreateInfo fci;
		fci.flags = vk::FenceCreateFlagBits::eSignaled;
		this->cmdBufferFences.push_back(this->device->createFence(fci));

		vk::CommandBuffer cb = *this->cmdBufs[i];
		vk::CommandBufferBeginInfo cbbi;
		cb.begin(cbbi);
		cb.end();
	}

	timestampPeriod = physDevProps.limits.timestampPeriod;

	vk::QueryPoolCreateInfo qpci{};
	qpci.queryType = vk::QueryType::eTimestamp;
	qpci.queryCount = 2;
	queryPool = device->createQueryPoolUnique(qpci);

	*success = true;
}

// Quite a lot of resources are dependent on either the number of images
// there are in the swap chain or the swapchain itself, so they need to be
// recreated whenever the swap chain changes.
void VKRenderer::createSCDependents() {
	auto memoryProps = physicalDevice.getMemoryProperties();

	this->depthStencilImage = vku::DepthStencilImage(*this->device, memoryProps, this->width, this->height, vk::Format::eD32Sfloat);

	//createFramebuffers();

	vku::PipelineMaker pm{ this->width, this->height };
	pm.shader(vk::ShaderStageFlagBits::eFragment, fragmentShader);
	pm.shader(vk::ShaderStageFlagBits::eVertex, vertexShader);
	pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
	pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
	pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal));
	pm.vertexAttribute(2, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, tangent));
	pm.vertexAttribute(3, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
	pm.cullMode(vk::CullModeFlagBits::eBack);
	pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);
	this->pipeline = pm.createUnique(*this->device, *this->pipelineCache, *this->pipelineLayout, *this->renderPass);

	vk::ImageCreateInfo ici;
	ici.imageType = vk::ImageType::e2D;
	ici.extent = vk::Extent3D{ width, height, 1 };
	ici.arrayLayers = 1;
	ici.mipLevels = 1;
	ici.format = vk::Format::eR16G16B16A16Sfloat;
	ici.initialLayout = vk::ImageLayout::eUndefined;
	ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
	polyImage = vku::GenericImage(*device, memoryProps, ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, false);

	ici.format = vk::Format::eR8G8B8A8Unorm;
	ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc;
	finalPrePresent = vku::GenericImage(*device, memoryProps, ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, false);
	vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [this](vk::CommandBuffer cmdBuf) {
		polyImage.setLayout(cmdBuf, vk::ImageLayout::eColorAttachmentOptimal);
		finalPrePresent.setLayout(cmdBuf, vk::ImageLayout::eColorAttachmentOptimal);
	});

	vk::ImageView attachments[2] = { polyImage.imageView(), depthStencilImage.imageView() };
	vk::FramebufferCreateInfo fci;
	fci.attachmentCount = 2;
	fci.pAttachments = attachments;
	fci.width = this->width;
	fci.height = this->height;
	fci.renderPass = *this->renderPass;
	fci.layers = 1;
	renderFb = device->createFramebufferUnique(fci);

	vk::ImageView finalImageView = finalPrePresent.imageView();
	fci.attachmentCount = 1;
	fci.pAttachments = &finalImageView;
	fci.renderPass = *imguiRenderPass;
	finalPrePresentFB = device->createFramebufferUnique(fci);
	updateTonemapDescriptors();
}

void VKRenderer::recreateSwapchain() {
	// Wait for current frame to finish
	device->waitIdle();

	// Check width/height - if it's 0, just ignore it
	auto surfaceCaps = this->physicalDevice.getSurfaceCapabilitiesKHR(this->surface);
	this->width = surfaceCaps.currentExtent.width;
	this->height = surfaceCaps.currentExtent.height;

	if (width == 0 || height == 0)
		return;

	std::unique_ptr<Swapchain> oldSwapchain = std::move(swapchain);

	createSwapchain(*oldSwapchain->getSwapchain());

	pipeline.reset();
	framebuffers.clear();
	oldSwapchain.reset();
	imageAcquire.reset();
	vk::SemaphoreCreateInfo sci;
	imageAcquire = device->createSemaphoreUnique(sci);

	createSCDependents();
}

void VKRenderer::presentNothing(uint32_t imageIndex) {
	vk::Semaphore waitSemaphore = *imageAcquire;

	vk::PresentInfoKHR presentInfo;
	vk::SwapchainKHR cSwapchain = *swapchain->getSwapchain();
	presentInfo.pSwapchains = &cSwapchain;
	presentInfo.swapchainCount = 1;
	presentInfo.pImageIndices = &imageIndex;

	presentInfo.pWaitSemaphores = &waitSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	vk::CommandBufferBeginInfo cbbi;
	auto& cmdBuf = cmdBufs[imageIndex];
	cmdBuf->begin(cbbi);
	cmdBuf->end();
	vk::SubmitInfo submitInfo;
	vk::CommandBuffer cCmdBuf = *cmdBuf;
	submitInfo.pCommandBuffers = &cCmdBuf;
	submitInfo.commandBufferCount = 1;
	device->getQueue(presentQueueFamilyIdx, 0).submit(submitInfo, cmdBufferFences[imageIndex]);

	device->getQueue(presentQueueFamilyIdx, 0).presentKHR(presentInfo);
}

void imageBarrier(vk::CommandBuffer& cb, vk::Image image, vk::ImageLayout layout, vk::AccessFlags srcMask, vk::AccessFlags dstMask, vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask, vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor) {
	vk::ImageMemoryBarrier imageMemoryBarriers = {};
	imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarriers.oldLayout = layout;
	imageMemoryBarriers.newLayout = layout;
	imageMemoryBarriers.image = image;
	imageMemoryBarriers.subresourceRange = { aspectMask, 0, 1, 0, 1 };

	// Put barrier on top
	vk::DependencyFlags dependencyFlags{};

	imageMemoryBarriers.srcAccessMask = srcMask;
	imageMemoryBarriers.dstAccessMask = dstMask;
	auto memoryBarriers = nullptr;
	auto bufferMemoryBarriers = nullptr;
	cb.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, memoryBarriers, bufferMemoryBarriers, imageMemoryBarriers);
}

void VKRenderer::doTonemap(vk::UniqueCommandBuffer& cmdBuf, uint32_t imageIndex) {
	finalPrePresent.setLayout(*cmdBuf, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderWrite);

	imageBarrier(*cmdBuf, polyImage.image(), vk::ImageLayout::eGeneral, vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader);

	cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eCompute, *tonemapPipelineLayout, 0, tonemapDescriptorSet, nullptr);
	cmdBuf->bindPipeline(vk::PipelineBindPoint::eCompute, *tonemapPipeline);
	cmdBuf->dispatch((width + 15) / 16, (height + 15) / 16, 1);

	imageBarrier(*cmdBuf, finalPrePresent.image(), vk::ImageLayout::eGeneral, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eColorAttachmentWrite, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eColorAttachmentOutput);

	std::array<float, 4> clearColorValue{ 0.0f, 0.0f, 0.0f, 1 };
	std::array<vk::ClearValue, 1> clearColours{ vk::ClearValue{clearColorValue} };
	vk::RenderPassBeginInfo rpbi;
	rpbi.renderPass = *imguiRenderPass;
	rpbi.framebuffer = *finalPrePresentFB;
	rpbi.renderArea = vk::Rect2D{ {0, 0}, {width, height} };
	rpbi.clearValueCount = clearColours.size();
	rpbi.pClearValues = clearColours.data();
	cmdBuf->beginRenderPass(rpbi, vk::SubpassContents::eInline);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmdBuf);
	cmdBuf->endRenderPass();

	// account for implicit renderpass transition
	finalPrePresent.setCurrentLayout(vk::ImageLayout::eTransferSrcOptimal);
	
}

void VKRenderer::renderPolys(vk::UniqueCommandBuffer& cmdBuf, entt::registry& reg, uint32_t imageIndex, Camera& cam) {
	// Fast path clear values for AMD
	std::array<float, 4> clearColorValue{ 0.0f, 0.0f, 0.0f, 1 };
	vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
	std::array<vk::ClearValue, 2> clearColours{ vk::ClearValue{clearColorValue}, clearDepthValue };
	vk::RenderPassBeginInfo rpbi;

	rpbi.renderPass = *renderPass;
	rpbi.framebuffer = *renderFb;
	rpbi.renderArea = vk::Rect2D{ {0, 0}, {width, height} };
	rpbi.clearValueCount = (uint32_t)clearColours.size();
	rpbi.pClearValues = clearColours.data();

	cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets[0], nullptr);
	cmdBuf->beginRenderPass(rpbi, vk::SubpassContents::eInline);
	cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

	reg.view<Transform, WorldObject>().each([this, &cmdBuf, &cam](auto ent, Transform& transform, WorldObject& obj) {
		auto meshPos = loadedMeshes.find(obj.mesh);

		if (meshPos == loadedMeshes.end()) {
			// Haven't loaded the mesh yet
			return;
		}

		StandardPushConstants pushConst{ glm::vec4(cam.position, 0.0f), transform.getMatrix() };
		cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
		cmdBuf->bindVertexBuffers(0, meshPos->second.vb.buffer(), vk::DeviceSize(0));
		cmdBuf->bindIndexBuffer(meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
		cmdBuf->drawIndexed(meshPos->second.indexCount, 1, 0, 0, 0);
	});
	cmdBuf->endRenderPass();
}

void VKRenderer::updateTonemapDescriptors() {
	vku::DescriptorSetUpdater dsu;
	dsu.beginDescriptorSet(tonemapDescriptorSet);
	dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
	dsu.image(*albedoSampler, finalPrePresent.imageView(), vk::ImageLayout::eGeneral);
	dsu.beginImages(1, 0, vk::DescriptorType::eStorageImage);
	dsu.image(*albedoSampler, polyImage.imageView(), vk::ImageLayout::eGeneral);
	dsu.update(*device);
}

void VKRenderer::frame(Camera& cam, entt::registry& reg) {
	// No point rendering if it's not going to be shown

	uint32_t imageIndex = 0;
	vk::Result nextImageRes = swapchain->acquireImage(*device, *imageAcquire, &imageIndex);

	if ((nextImageRes == vk::Result::eSuboptimalKHR || nextImageRes == vk::Result::eErrorOutOfDateKHR) && width != 0 && height != 0) {
		recreateSwapchain();
		// acquire image from new swapchain
		swapchain->acquireImage(*device, *imageAcquire, &imageIndex);
	}

	device->waitForFences(cmdBufferFences[imageIndex], 1, std::numeric_limits<uint64_t>::max());
	device->resetFences(cmdBufferFences[imageIndex]);

	if (width == 0 || height == 0) {
		// If the window has a width or height of zero, submit a blank command buffer to signal the fences.
		// This avoids unnecessary GPU work when the user can't see the output anyway.
		presentNothing(imageIndex);
		return;
	}

	VP vp;
	vp.view = cam.getViewMatrix();
	vp.projection = cam.getProjectionMatrix((float)width / (float)height);
	LightUB lub;
	lub.pack0.x = 1;
	lub.lights[0] = PackedLight{ glm::vec4(1.0f, 1.0f, 1.0f, 2.0f), glm::normalize(glm::vec4(0.0f, 0.5f, 0.5f, 0.0f)), glm::vec4(0.0f, 0.0f, -0.1f, 0.0f) };

	auto& cmdBuf = cmdBufs[imageIndex];

	vk::CommandBufferBeginInfo cbbi;

	cmdBuf->begin(cbbi);
	cmdBuf->resetQueryPool(*queryPool, 0, 2);
	cmdBuf->writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0);
	cmdBuf->updateBuffer<LightUB>(lightsUB.buffer(), 0, lub);
	cmdBuf->updateBuffer<VP>(vpUB.buffer(), 0, vp);

	vpUB.barrier(
		*cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eVertexShader,
		vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eUniformRead,
		graphicsQueueFamilyIdx, graphicsQueueFamilyIdx);

	lightsUB.barrier(
		*cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eFragmentShader,
		vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eUniformRead,
		graphicsQueueFamilyIdx, graphicsQueueFamilyIdx);

	renderPolys(cmdBuf, reg, imageIndex, cam);
	doTonemap(cmdBuf, imageIndex);
	
	vku::transitionLayout(*cmdBuf, swapchain->images[imageIndex], vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite);
	vk::ImageBlit imageBlit;
	imageBlit.srcOffsets[1] = imageBlit.dstOffsets[1] = { (int)width, (int)height, 1 };
	imageBlit.dstSubresource = imageBlit.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
	cmdBuf->blitImage(finalPrePresent.image(), vk::ImageLayout::eTransferSrcOptimal, swapchain->images[imageIndex], vk::ImageLayout::eTransferDstOptimal, imageBlit, vk::Filter::eNearest);
	vku::transitionLayout(*cmdBuf, swapchain->images[imageIndex], vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead);
	
	cmdBuf->writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *queryPool, 1);
	cmdBuf->end();

	vk::SubmitInfo submit;
	submit.waitSemaphoreCount = 1;
	vk::Semaphore cImageAcquire = *imageAcquire;
	submit.pWaitSemaphores = &cImageAcquire;
	vk::PipelineStageFlags waitStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	submit.pWaitDstStageMask = &waitStages;
	submit.commandBufferCount = 1;
	vk::CommandBuffer cCmdBuf = *cmdBuf;
	submit.pCommandBuffers = &cCmdBuf;

	vk::Semaphore waitSemaphore = *commandComplete;

	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &waitSemaphore;
	device->getQueue(graphicsQueueFamilyIdx, 0).submit(1, &submit, cmdBufferFences[imageIndex]);

	vk::PresentInfoKHR presentInfo;
	vk::SwapchainKHR cSwapchain = *swapchain->getSwapchain();
	presentInfo.pSwapchains = &cSwapchain;
	presentInfo.swapchainCount = 1;
	presentInfo.pImageIndices = &imageIndex;

	presentInfo.pWaitSemaphores = &waitSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	vk::Result presentResult = device->getQueue(presentQueueFamilyIdx, 0).presentKHR(presentInfo);

	std::array<std::uint64_t, 2> timeStamps = { {0} };
	device->getQueryPoolResults<std::uint64_t>(
		*queryPool, 0, timeStamps.size(),
		timeStamps, sizeof(std::uint64_t),
		vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait
	);
	lastRenderTimeTicks = timeStamps[1] - timeStamps[0];
}

void VKRenderer::preloadMesh(AssetID id) {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	PhysFS::ifstream meshFileStream(g_assetDB.openDataFile(id));
	loadMesh(vertices, indices, meshFileStream);

	auto memProps = physicalDevice.getMemoryProperties();
	LoadedMeshData lmd;
	lmd.indexType = vk::IndexType::eUint32;
	lmd.indexCount = indices.size();
	lmd.ib = vku::IndexBuffer{ *device, memProps, indices.size() * sizeof(uint32_t) };
	lmd.ib.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), indices);
	lmd.vb = vku::VertexBuffer{ *device, memProps, vertices.size() * sizeof(Vertex) };
	lmd.vb.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), vertices);
	loadedMeshes.insert({ id, std::move(lmd) });
}

VKRenderer::~VKRenderer() {
	if (this->device) {
		this->device->waitIdle();
		// Some stuff has to be manually destroyed

		for (auto& fence : this->cmdBufferFences) {
			this->device->destroyFence(fence);
		}

		this->swapchain.reset();

		this->instance->destroySurfaceKHR(this->surface);
	}
}