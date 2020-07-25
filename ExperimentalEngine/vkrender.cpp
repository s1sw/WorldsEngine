#define VMA_IMPLEMENTATION
#include "PCH.hpp"
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
#ifdef TRACY_ENABLE
#include "tracy/Tracy.hpp"
#include "tracy/TracyC.h"
#include "tracy/TracyVulkan.hpp"
#endif
#include "XRInterface.hpp"
#include "RenderPasses.hpp"

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

RenderImageHandle VKRenderer::createRTResource(RTResourceCreateInfo resourceCreateInfo, const char* debugName) {
    auto memProps = physicalDevice.getMemoryProperties();
    RenderTextureResource rtr;
    rtr.image = vku::GenericImage{ *device, memProps, resourceCreateInfo.ici, resourceCreateInfo.viewType, resourceCreateInfo.aspectFlags, false, debugName };
    rtr.aspectFlags = resourceCreateInfo.aspectFlags;

    RenderImageHandle handle = lastHandle++;
    rtResources.insert({ handle, std::move(rtr) });
    return handle;
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
        vk::ImageView attachments[1] = { swapchain->imageViews[i] };
        vk::FramebufferCreateInfo fci;
        fci.attachmentCount = 1;
        fci.pAttachments = attachments;
        fci.width = this->width;
        fci.height = this->height;
        fci.renderPass = *this->imguiRenderPass;
        fci.layers = 1;
        this->framebuffers.push_back(this->device->createFramebufferUnique(fci));
    }
}

void VKRenderer::loadTex(const char* path, int index) {
    auto memProps = physicalDevice.getMemoryProperties();
    int x, y, channelsInFile;
    stbi_uc* dat = stbi_load(path, &x, &y, &channelsInFile, 4);

    if (dat == nullptr) {
    }
    textures[index].present = true;
    textures[index].tex = vku::TextureImage2D{ *device, memProps, (uint32_t)x, (uint32_t)y, 1, vk::Format::eR8G8B8A8Srgb };

    std::vector<uint8_t> albedoDat(dat, dat + ((size_t)x * y * 4));

    textures[index].tex.upload(*device, allocator, albedoDat, *commandPool, memProps, device->getQueue(graphicsQueueFamilyIdx, 0));
}

void VKRenderer::loadAlbedo() {
    loadTex("albedo.png", 0);
    loadTex("terrain.png", 1);
}

VKRenderer::VKRenderer(RendererInitInfo& initInfo, bool* success) 
    : window(initInfo.window)
    , frameIdx(0)
    , lastHandle(0)
    , polyImage(std::numeric_limits<uint32_t>::max())
    , shadowmapImage(std::numeric_limits<uint32_t>::max())
    , shadowmapRes(1024)
    , enableVR(initInfo.enableVR) {
    msaaSamples = vk::SampleCountFlagBits::e4;
    numMSAASamples = 4;

    vku::InstanceMaker instanceMaker;
    instanceMaker.apiVersion(VK_API_VERSION_1_2);
    unsigned int extCount;
    SDL_Vulkan_GetInstanceExtensions(window, &extCount, nullptr);

    std::vector<const char*> names(extCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extCount, names.data());

    for (auto extName : names)
        instanceMaker.extension(extName);

    for (auto& extName : initInfo.additionalInstanceExtensions)
        instanceMaker.extension(extName.c_str());

#ifndef NDEBUG
    instanceMaker.layer("VK_LAYER_KHRONOS_validation");
    instanceMaker.extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
    instanceMaker.extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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
    for (uint32_t i = 0; i < memoryProps.memoryHeapCount; i++) {
        auto& heap = memoryProps.memoryHeaps[i];
        totalVram += heap.size;
        std::cout << "Heap " << i << ": " << heap.size / 1024 / 1024 << "MB\n";
    }

    for (uint32_t i = 0; i < memoryProps.memoryTypeCount; i++) {
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

    // Look for a queue family with both graphics and
    // compute first.
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

    for (auto& ext : initInfo.additionalDeviceExtensions) {
        dm.extension(ext.c_str());
    }

    vk::PhysicalDeviceFeatures supportedFeatures = physicalDevice.getFeatures();
    if (!supportedFeatures.shaderStorageImageMultisample) {
        *success = false;
        std::cout << "Missing shaderStorageImageMultisample\n";
        return;
    }

    if (!supportedFeatures.fragmentStoresAndAtomics) {
        std::cout << "Missing fragmentStoresAndAtomics, editor selection won't work\n";
    }

    if (!supportedFeatures.fillModeNonSolid) {
        std::cout << "Missing fillModeNonSolid, selection wireframe won't show in editor\n";
    }

    if (!supportedFeatures.wideLines) {
        std::cout << "Missing wideLines, selection wireframe may be thin or missing\n";
    }

    vk::PhysicalDeviceFeatures features;
    features.shaderStorageImageMultisample = true;
    features.fragmentStoresAndAtomics = true;
    features.fillModeNonSolid = true;
    features.wideLines = true;
    dm.setFeatures(features);

    vk::PhysicalDeviceDescriptorIndexingFeatures diFeatures;
    diFeatures.descriptorBindingPartiallyBound = true;
    diFeatures.runtimeDescriptorArray = true;
    dm.setPNext(&diFeatures);
    if (this->computeQueueFamilyIdx != this->graphicsQueueFamilyIdx) dm.queue(this->computeQueueFamilyIdx);
    this->device = dm.createUnique(this->physicalDevice);

    VmaAllocatorCreateInfo allocatorCreateInfo;
    memset(&allocatorCreateInfo, 0, sizeof(allocatorCreateInfo));
    allocatorCreateInfo.device = *device;
    allocatorCreateInfo.frameInUseCount = 0;
    allocatorCreateInfo.instance = *instance;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorCreateInfo.flags = 0;
    vmaCreateAllocator(&allocatorCreateInfo, &allocator);

    vk::PipelineCacheCreateInfo pipelineCacheInfo{};
    this->pipelineCache = this->device->createPipelineCacheUnique(pipelineCacheInfo);

    std::vector<vk::DescriptorPoolSize> poolSizes;
    poolSizes.emplace_back(vk::DescriptorType::eUniformBuffer, 1024);
    poolSizes.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1024);
    poolSizes.emplace_back(vk::DescriptorType::eStorageBuffer, 1024);

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

    loadAlbedo();

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
#ifdef TRACY_ENABLE
    for (auto& cmdBuf : cmdBufs) {
        tracyContexts.push_back(tracy::CreateVkContext(physicalDevice, *device, device->getQueue(graphicsQueueFamilyIdx, 0), *cmdBufs[0]));
    }
#endif

    if (enableVR) {
        XrGraphicsBindingVulkanKHR graphicsBinding;
        graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
        graphicsBinding.instance = *instance;
        graphicsBinding.queueFamilyIndex = graphicsQueueFamilyIdx;
        graphicsBinding.queueIndex = 0;
        graphicsBinding.device = *device;
        graphicsBinding.physicalDevice = physicalDevice;
        graphicsBinding.next = nullptr;
        
        initInfo.xrInterface->createSession(graphicsBinding);
    }
}

// Quite a lot of resources are dependent on either the number of images
// there are in the swap chain or the swapchain itself, so they need to be
// recreated whenever the swap chain changes.
void VKRenderer::createSCDependents() {
    auto memoryProps = physicalDevice.getMemoryProperties();

    if (rtResources.count(polyImage) != 0) {
        rtResources.erase(polyImage);
    }

    if (rtResources.count(depthStencilImage) != 0) {
        rtResources.erase(depthStencilImage);
    }

    if (rtResources.count(imguiImage) != 0) {
        rtResources.erase(imguiImage);
    }

    vk::ImageCreateInfo ici;
    ici.imageType = vk::ImageType::e2D;
    ici.extent = vk::Extent3D{ width, height, 1 };
    ici.arrayLayers = enableVR ? 2 : 1;
    ici.mipLevels = 1;
    ici.format = vk::Format::eR16G16B16A16Sfloat;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    ici.samples = msaaSamples;
    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;

    RTResourceCreateInfo polyCreateInfo{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };
    polyImage = createRTResource(polyCreateInfo, "Poly Image");

    ici.format = vk::Format::eD32Sfloat;
    ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    RTResourceCreateInfo depthCreateInfo{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eDepth };
    depthStencilImage = createRTResource(depthCreateInfo, "Depth Stencil Image");

    ici.format = vk::Format::eR8G8B8A8Unorm;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;

    RTResourceCreateInfo imguiImageCreateInfo{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };
    imguiImage = createRTResource(imguiImageCreateInfo, "ImGui Image");

    vk::ImageCreateInfo shadowmapIci;
    shadowmapIci.imageType = vk::ImageType::e2D;
    shadowmapIci.extent = vk::Extent3D{ shadowmapRes, shadowmapRes, 1 };
    shadowmapIci.arrayLayers = 1;
    shadowmapIci.mipLevels = 1;
    shadowmapIci.format = vk::Format::eD32Sfloat;
    shadowmapIci.initialLayout = vk::ImageLayout::eUndefined;
    shadowmapIci.samples = vk::SampleCountFlagBits::e1;
    shadowmapIci.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;

    RTResourceCreateInfo shadowmapCreateInfo{ shadowmapIci, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eDepth };
    shadowmapImage = createRTResource(shadowmapCreateInfo, "Shadowmap Image");

    graphSolver.clear();
    {
        auto srp = new ShadowmapRenderPass(shadowmapImage);
        graphSolver.addNode(srp);
    }

    {
        auto prp = new PolyRenderPass(depthStencilImage, polyImage, shadowmapImage, true);
        currentPRP = prp;
        graphSolver.addNode(prp);
    }

    {
        auto irp = new ImGuiRenderPass(imguiImage);
        graphSolver.addNode(irp);
    }

    ici.arrayLayers = 1;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.format = vk::Format::eR8G8B8A8Unorm;
    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc;

    //finalPrePresent = vku::GenericImage(*device, memoryProps, ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, false);
    RTResourceCreateInfo finalPrePresentCI{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };
    finalPrePresent = createRTResource(finalPrePresentCI, "Final Pre-Present Image");

    {
        auto tonemapRP = new TonemapRenderPass(polyImage, imguiImage, finalPrePresent);
        graphSolver.addNode(tonemapRP);
    }

    vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [this](vk::CommandBuffer cmdBuf) {
        rtResources.at(polyImage).image.setLayout(cmdBuf, vk::ImageLayout::eGeneral);
        rtResources.at(finalPrePresent).image.setLayout(cmdBuf, vk::ImageLayout::eTransferSrcOptimal);
        });

    PassSetupCtx psc{ physicalDevice, *device, *pipelineCache, *descriptorPool, *commandPool, *instance, allocator, graphicsQueueFamilyIdx, GraphicsSettings{numMSAASamples, (int32_t)shadowmapRes}, textures, rtResources, swapchain->images.size() };

    auto solved = graphSolver.solve();

    for (auto& node : solved) {
        node->setup(psc);
    }
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

void VKRenderer::imageBarrier(vk::CommandBuffer& cb, ImageBarrier& ib) {
    vk::ImageMemoryBarrier imageMemoryBarriers = {};
    imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.oldLayout = ib.oldLayout;
    imageMemoryBarriers.newLayout = ib.newLayout;
    imageMemoryBarriers.image = rtResources.at(ib.handle).image.image();
    imageMemoryBarriers.subresourceRange = { ib.aspectMask, 0, 1, 0, 1 };

    // Put barrier on top
    vk::DependencyFlags dependencyFlags{};

    imageMemoryBarriers.srcAccessMask = ib.srcMask;
    imageMemoryBarriers.dstAccessMask = ib.dstMask;
    auto memoryBarriers = nullptr;
    auto bufferMemoryBarriers = nullptr;
    cb.pipelineBarrier(ib.srcStage, ib.dstStage, dependencyFlags, memoryBarriers, bufferMemoryBarriers, imageMemoryBarriers);
}

vku::ShaderModule VKRenderer::loadShaderAsset(AssetID id) {
    PHYSFS_File* file = g_assetDB.openDataFile(id);
    size_t size = PHYSFS_fileLength(file);
    void* buffer = std::malloc(size);

    size_t readBytes = PHYSFS_readBytes(file, buffer, size);
    assert(readBytes == size);
    PHYSFS_close(file);

    vku::ShaderModule sm{ *device, static_cast<uint32_t*>(buffer), readBytes };
    std::free(buffer);
    return sm;
}

void VKRenderer::frame(Camera& cam, entt::registry& reg) {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    // No point rendering if it's not going to be shown
    currentPRP->setPickCoords(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);

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

    std::vector<RenderPass*> solvedNodes = graphSolver.solve();
    std::unordered_map<RenderImageHandle, vk::ImageAspectFlagBits> rtAspects;

    for (auto& pair : rtResources) {
        rtAspects.insert({ pair.first, pair.second.aspectFlags });
    }

    std::vector<std::vector<ImageBarrier>> barriers = graphSolver.createImageBarriers(solvedNodes, rtAspects);

    auto& cmdBuf = cmdBufs[imageIndex];

    RenderCtx rCtx{ cmdBuf, reg, imageIndex, cam, rtResources, width, height, loadedMeshes };

    // TODO: Pre-pass

    vk::CommandBufferBeginInfo cbbi;

    cmdBuf->begin(cbbi);
    cmdBuf->resetQueryPool(*queryPool, 0, 2);
    cmdBuf->writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0);

    PassSetupCtx psc{ physicalDevice, *device, *pipelineCache, *descriptorPool, *commandPool, *instance, allocator, graphicsQueueFamilyIdx, GraphicsSettings{numMSAASamples, (int32_t)shadowmapRes}, textures, rtResources, swapchain->images.size() };

    for (auto& node : solvedNodes) {
        node->prePass(psc, rCtx);
    }

    for (int i = 0; i < solvedNodes.size(); i++) {
        auto& node = solvedNodes[i];
        // Put in barriers for this node
        for (auto& barrier : barriers[i])
            imageBarrier(*cmdBuf, barrier);

        node->execute(rCtx);
    }

    vku::transitionLayout(*cmdBuf, swapchain->images[imageIndex],
        vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer,
        vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite);

    ::imageBarrier(*cmdBuf, rtResources.at(finalPrePresent).image.image(), vk::ImageLayout::eTransferSrcOptimal,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer);

    vk::ImageBlit imageBlit;
    imageBlit.srcOffsets[1] = imageBlit.dstOffsets[1] = { (int)width, (int)height, 1 };
    imageBlit.dstSubresource = imageBlit.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    cmdBuf->blitImage(
        rtResources.at(finalPrePresent).image.image(), vk::ImageLayout::eTransferSrcOptimal,
        swapchain->images[imageIndex], vk::ImageLayout::eTransferDstOptimal,
        imageBlit, vk::Filter::eNearest);

    vku::transitionLayout(*cmdBuf, swapchain->images[imageIndex],
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead);

    cmdBuf->writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *queryPool, 1);
#ifdef TRACY_ENABLE
    TracyVkCollect(tracyContexts[imageIndex], *cmdBuf);
#endif
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
    frameIdx++;
#ifdef TRACY_ENABLE
    FrameMark
#endif
}

void loadObj(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::istream& stream) {
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

void VKRenderer::preloadMesh(AssetID id) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    auto ext = g_assetDB.getAssetExtension(id);

    if (ext == ".obj") { // obj
        PhysFS::ifstream meshFileStream(g_assetDB.openDataFile(id));
        loadObj(vertices, indices, meshFileStream);
    } else if (ext == ".mdl") { // source model

    }

    auto memProps = physicalDevice.getMemoryProperties();
    LoadedMeshData lmd;
    lmd.indexType = vk::IndexType::eUint32;
    lmd.indexCount = indices.size();
    lmd.ib = vku::IndexBuffer{ *device, allocator, indices.size() * sizeof(uint32_t) };
    lmd.ib.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), indices);
    lmd.vb = vku::VertexBuffer{ *device, allocator, vertices.size() * sizeof(Vertex) };
    lmd.vb.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), vertices);
    loadedMeshes.insert({ id, std::move(lmd) });
}

void VKRenderer::uploadProcObj(ProceduralObject& procObj) {
    if (procObj.vertices.size() == 0) {
        procObj.visible = false;
        return;
    } else {
        procObj.visible = true;
    }
    device->waitIdle();
    auto memProps = physicalDevice.getMemoryProperties();
    procObj.indexType = vk::IndexType::eUint32;
    procObj.indexCount = procObj.indices.size();
    procObj.ib = vku::IndexBuffer{ *device, allocator, procObj.indices.size() * sizeof(uint32_t) };
    procObj.ib.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), procObj.indices);
    procObj.vb = vku::VertexBuffer{ *device, allocator, procObj.vertices.size() * sizeof(Vertex) };
    procObj.vb.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), procObj.vertices);
}

entt::entity VKRenderer::getPickedEnt() {
    return (entt::entity)currentPRP->getPickedEntity();
}

VKRenderer::~VKRenderer() {
    if (this->device) {
        this->device->waitIdle();
        // Some stuff has to be manually destroyed

        for (auto& fence : this->cmdBufferFences) {
            this->device->destroyFence(fence);
        }

        graphSolver.clear();

        for (auto& texSlot : textures) {
            texSlot.present = false;
            texSlot.tex = vku::TextureImage2D{};
        }
        rtResources.clear();
        loadedMeshes.clear();
        vmaDestroyAllocator(allocator);

        this->swapchain.reset();

        this->instance->destroySurfaceKHR(this->surface);
    }
}