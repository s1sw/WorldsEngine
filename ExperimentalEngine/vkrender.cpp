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
#include "tracy/Tracy.hpp"
#include "tracy/TracyC.h"
#include "tracy/TracyVulkan.hpp"
#include "XRInterface.hpp"
#include "RenderPasses.hpp"
#include "Input.hpp"
#include "OpenVRInterface.hpp"
#include <sajson.h>
#include "Fatal.hpp"

const bool vrValidationLayers = false;

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

    if (!enableVR) {
        renderWidth = width;
        renderHeight = height;
    }

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

VKRenderer::VKRenderer(const RendererInitInfo& initInfo, bool* success)
    : window(initInfo.window)
    , frameIdx(0)
    , lastHandle(0)
    , polyImage(std::numeric_limits<uint32_t>::max())
    , shadowmapImage(std::numeric_limits<uint32_t>::max())
    , shadowmapRes(1024)
    , enableVR(initInfo.enableVR)
    , vrPredictAmount(0.033f)
    , clearMaterialIndices(false) {
    msaaSamples = vk::SampleCountFlagBits::e4;
    numMSAASamples = 4;

    vku::InstanceMaker instanceMaker;
    instanceMaker.apiVersion(VK_API_VERSION_1_2);
    unsigned int extCount;
    SDL_Vulkan_GetInstanceExtensions(window, &extCount, nullptr);

    std::vector<const char*> names(extCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extCount, names.data());

    std::vector<std::string> instanceExtensions;

    for (auto extName : names)
        instanceExtensions.push_back(extName);

    for (auto& extName : initInfo.additionalInstanceExtensions)
        instanceExtensions.push_back(extName);

    if (initInfo.enableVR && initInfo.activeVrApi == VrApi::OpenVR) {
        OpenVRInterface* vrInterface = static_cast<OpenVRInterface*>(initInfo.vrInterface);
        auto vrInstExts = vrInterface->getVulkanInstanceExtensions();

        for (auto& extName : vrInstExts) {
            if (std::find_if(instanceExtensions.begin(), instanceExtensions.end(), [&extName](std::string val) { return val == extName; }) != instanceExtensions.end()) {
                continue;
            }
            instanceExtensions.push_back(extName);
        }
    }

    for (auto& v : vk::enumerateInstanceExtensionProperties()) {
        std::cout << "supported extension: " << v.extensionName << "\n";
    }

    for (auto& e : instanceExtensions) {
        std::cout << "activating extension: " << e << "\n";
        instanceMaker.extension(e.c_str());
    }

#ifndef NDEBUG
    if (!enableVR || vrValidationLayers) {
        instanceMaker.layer("VK_LAYER_KHRONOS_validation");
        instanceMaker.extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
#endif
    instanceMaker.extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    instanceMaker.applicationName("Experimental Game")
        .engineName("Experimental Engine")
        .applicationVersion(1)
        .engineVersion(1);

    this->instance = instanceMaker.createUnique();
#ifndef NDEBUG
    if (!enableVR || vrValidationLayers)
        dbgCallback = vku::DebugCallback(*this->instance);
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

    // Stupid workaround: putting this vector inside the if
    // causes it to go out of scope, making all the const char*
    // extension strings become invalid and screwing
    // everything up
    std::vector<std::string> vrDevExts;
    if (initInfo.enableVR && initInfo.activeVrApi == VrApi::OpenVR) {
        OpenVRInterface* vrInterface = static_cast<OpenVRInterface*>(initInfo.vrInterface);
        vrDevExts = vrInterface->getVulkanDeviceExtensions(physicalDevice);
        for (auto& extName : vrDevExts) {
            dm.extension(extName.c_str());
        }
    }

    vk::PhysicalDeviceFeatures supportedFeatures = physicalDevice.getFeatures();
    if (!supportedFeatures.shaderStorageImageMultisample) {
        *success = false;
        std::cout << "Missing shaderStorageImageMultisample\n";
        return;
    }

    if (!supportedFeatures.fragmentStoresAndAtomics) {
        std::cout << "Missing fragmentStoresAndAtomics\n";
    }

    if (!supportedFeatures.fillModeNonSolid) {
        std::cout << "Missing fillModeNonSolid\n";
    }

    if (!supportedFeatures.wideLines) {
        std::cout << "Missing wideLines\n";
    }

    vk::PhysicalDeviceFeatures features;
    features.shaderStorageImageMultisample = true;
    features.fragmentStoresAndAtomics = true;
    features.fillModeNonSolid = true;
    features.wideLines = true;
    features.samplerAnisotropy = true;
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

    if (initInfo.activeVrApi == VrApi::OpenVR) {
        OpenVRInterface* vrInterface = static_cast<OpenVRInterface*>(initInfo.vrInterface);
        vrInterface->getRenderResolution(&renderWidth, &renderHeight);
    }

    auto vkCtx = std::make_shared<VulkanCtx>(VulkanCtx{
        physicalDevice,
        *device,
        *pipelineCache,
        *descriptorPool,
        *commandPool,
        *instance,
        allocator,
        graphicsQueueFamilyIdx,
        GraphicsSettings {
            numMSAASamples,
            (int)shadowmapRes,
            enableVR
        },
        width, height,
        renderWidth, renderHeight
    });

    texSlots = std::make_unique<TextureSlots>(vkCtx);
    matSlots = std::make_unique<MaterialSlots>(vkCtx, *texSlots);

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
        if (initInfo.activeVrApi == VrApi::OpenXR) {
            XrGraphicsBindingVulkanKHR graphicsBinding;
            graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
            graphicsBinding.instance = *instance;
            graphicsBinding.queueFamilyIndex = graphicsQueueFamilyIdx;
            graphicsBinding.queueIndex = 0;
            graphicsBinding.device = *device;
            graphicsBinding.physicalDevice = physicalDevice;
            graphicsBinding.next = nullptr;

            ((XRInterface*)initInfo.vrInterface)->createSession(graphicsBinding);
        } else if (initInfo.activeVrApi == VrApi::OpenVR) {
            vr::VRCompositor()->SetExplicitTimingMode(vr::EVRCompositorTimingMode::VRCompositorTimingMode_Explicit_RuntimePerformsPostPresentHandoff);
        }

        vrInterface = initInfo.vrInterface;
        vrApi = initInfo.activeVrApi;
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

    if (rtResources.count(finalPrePresent) != 0) {
        rtResources.erase(finalPrePresent);
    }

    if (rtResources.count(finalPrePresentR) != 0) {
        rtResources.erase(finalPrePresentR);
    }

    vk::ImageCreateInfo ici;
    ici.imageType = vk::ImageType::e2D;
    ici.extent = vk::Extent3D{ renderWidth, renderHeight, 1 };
    ici.arrayLayers = enableVR ? 2 : 1;
    ici.mipLevels = 1;
    ici.format = vk::Format::eR16G16B16A16Sfloat;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    ici.samples = msaaSamples;
    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;

    RTResourceCreateInfo polyCreateInfo{ ici, enableVR ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };
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

    delete irp;
    graphSolver.clear();
    {
        auto srp = new ShadowmapRenderPass(shadowmapImage);
        graphSolver.addNode(srp);
    }

    {
        auto prp = new PolyRenderPass(depthStencilImage, polyImage, shadowmapImage, !enableVR);
        currentPRP = prp;
        graphSolver.addNode(prp);
    }

    ici.arrayLayers = 1;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.format = vk::Format::eR8G8B8A8Unorm;
    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc;

    RTResourceCreateInfo finalPrePresentCI{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };
    finalPrePresent = createRTResource(finalPrePresentCI, "Final Pre-Present Image");

    if (enableVR) {
        finalPrePresentR = createRTResource(finalPrePresentCI, "Final Pre-Present Image (Right Eye)");
    }

    PassSetupCtx psc{ physicalDevice, *device, *pipelineCache, *descriptorPool, *commandPool, *instance, allocator, graphicsQueueFamilyIdx, GraphicsSettings{numMSAASamples, (int32_t)shadowmapRes, enableVR}, &texSlots, rtResources, (int)swapchain->images.size(), enableVR };

    auto tonemapRP = new TonemapRenderPass(polyImage, finalPrePresent);
    graphSolver.addNode(tonemapRP);

    irp = new ImGuiRenderPass(finalPrePresent);

    vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [this](vk::CommandBuffer cmdBuf) {
        rtResources.at(polyImage).image.setLayout(cmdBuf, vk::ImageLayout::eGeneral);
        rtResources.at(finalPrePresent).image.setLayout(cmdBuf, vk::ImageLayout::eTransferSrcOptimal);
        if (enableVR) {
            rtResources.at(finalPrePresentR).image.setLayout(cmdBuf, vk::ImageLayout::eTransferSrcOptimal);
        }
        });

    auto solved = graphSolver.solve();

    for (auto& node : solved) {
        node->setup(psc);
    }

    if (enableVR) {
        tonemapRP->setRightFinalImage(psc, finalPrePresentR);
    }

    irp->setup(psc);
}

void VKRenderer::recreateSwapchain() {
    // Wait for current frame to finish
    device->waitIdle();

    // Check width/height - if it's 0, just ignore it
    auto surfaceCaps = this->physicalDevice.getSurfaceCapabilitiesKHR(this->surface);
    this->width = surfaceCaps.currentExtent.width;
    this->height = surfaceCaps.currentExtent.height;

    if (!enableVR) {
        renderWidth = width;
        renderHeight = height;
    }

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

void imageBarrier(vk::CommandBuffer& cb, vk::Image image, vk::ImageLayout layout, vk::AccessFlags srcMask, vk::AccessFlags dstMask, vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask, vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor, uint32_t numLayers = 1) {
    vk::ImageMemoryBarrier imageMemoryBarriers = {};
    imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.oldLayout = layout;
    imageMemoryBarriers.newLayout = layout;
    imageMemoryBarriers.image = image;
    imageMemoryBarriers.subresourceRange = { aspectMask, 0, 1, 0, numLayers };

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
    imageMemoryBarriers.subresourceRange = { ib.aspectMask, 0, 1, 0, rtResources.at(ib.handle).image.info().arrayLayers };

    // Put barrier on top
    vk::DependencyFlags dependencyFlags{};

    imageMemoryBarriers.srcAccessMask = ib.srcMask;
    imageMemoryBarriers.dstAccessMask = ib.dstMask;
    auto memoryBarriers = nullptr;
    auto bufferMemoryBarriers = nullptr;
    cb.pipelineBarrier(ib.srcStage, ib.dstStage, dependencyFlags, memoryBarriers, bufferMemoryBarriers, imageMemoryBarriers);
}

vku::ShaderModule VKRenderer::loadShaderAsset(AssetID id) {
    PHYSFS_File* file = g_assetDB.openAssetFileRead(id);
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
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    currentPRP->setPickCoords(mx, my);

    uint32_t imageIndex = 0;
    vk::Result nextImageRes = swapchain->acquireImage(*device, *imageAcquire, &imageIndex);

    if ((nextImageRes == vk::Result::eSuboptimalKHR || nextImageRes == vk::Result::eErrorOutOfDateKHR) && width != 0 && height != 0) {
        recreateSwapchain();
        // acquire image from new swapchain
        swapchain->acquireImage(*device, *imageAcquire, &imageIndex);
    }

    device->waitForFences(cmdBufferFences[imageIndex], 1, std::numeric_limits<uint64_t>::max());
    device->resetFences(cmdBufferFences[imageIndex]);
    destroyTempTexBuffers(imageIndex);

    if ((width == 0 || height == 0) && !enableVR) {
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

    RenderCtx rCtx{ cmdBuf, reg, imageIndex, cam, rtResources, renderWidth, renderHeight, loadedMeshes };
    rCtx.enableVR = enableVR;
    rCtx.materialSlots = &matSlots;
    rCtx.textureSlots = &texSlots;
    rCtx.viewPos = cam.position;

#ifdef TRACY_ENABLE
    rCtx.tracyContexts = &tracyContexts;
#endif

    if (enableVR) {
        if (vrApi == VrApi::OpenVR) {
            OpenVRInterface* ovrInterface = static_cast<OpenVRInterface*>(vrInterface);
            rCtx.vrProjMats[0] = ovrInterface->getProjMat(vr::EVREye::Eye_Left, 0.01f, 100.0f);
            rCtx.vrProjMats[1] = ovrInterface->getProjMat(vr::EVREye::Eye_Right, 0.01f, 100.0f);
            rCtx.vrViewMats[0] = ovrInterface->getViewMat(vr::EVREye::Eye_Left);
            rCtx.vrViewMats[1] = ovrInterface->getViewMat(vr::EVREye::Eye_Right);
        }
    }

    vk::CommandBufferBeginInfo cbbi;

    cmdBuf->begin(cbbi);
    cmdBuf->resetQueryPool(*queryPool, 0, 2);
    cmdBuf->writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0);

    texSlots->setUploadCommandBuffer(*cmdBuf, imageIndex);

    if (clearMaterialIndices) {
        reg.view<WorldObject>().each([](entt::entity, WorldObject& wo) {
            wo.materialIdx = ~0u;
            });
        clearMaterialIndices = false;
    }

    PassSetupCtx psc{ physicalDevice, *device, *pipelineCache, *descriptorPool, *commandPool, *instance, allocator, graphicsQueueFamilyIdx, GraphicsSettings{numMSAASamples, (int32_t)shadowmapRes, enableVR}, &texSlots, rtResources, (int)swapchain->images.size(), enableVR };

    // Upload any necessary materials
    reg.view<WorldObject>().each([this, &rCtx](auto ent, WorldObject& wo) {
        if (wo.materialIdx == ~0u) {
            rCtx.reuploadMats = true;
            wo.materialIdx = matSlots->loadOrGet(wo.material);
        }
        });


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

    irp->execute(rCtx);

    vku::transitionLayout(*cmdBuf, swapchain->images[imageIndex],
        vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer,
        vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite);

    ::imageBarrier(*cmdBuf, rtResources.at(finalPrePresent).image.image(), vk::ImageLayout::eTransferSrcOptimal,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer);

    if (enableVR) {
        vku::transitionLayout(*cmdBuf, rtResources.at(finalPrePresentR).image.image(),
            vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead);
    }

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

    if (enableVR && vrApi == VrApi::OpenVR) {
        OpenVRInterface* ovrInterface = static_cast<OpenVRInterface*>(vrInterface);

        vr::TrackedDevicePose_t pose;
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::ETrackingUniverseOrigin::TrackingUniverseStanding, vrPredictAmount, &pose, 1);

        glm::mat4 viewMats[2];
        viewMats[0] = ovrInterface->getViewMat(vr::EVREye::Eye_Left);
        viewMats[1] = ovrInterface->getViewMat(vr::EVREye::Eye_Right);

        glm::vec3 viewPos[2];

        for (int i = 0; i < 2; i++) {
            viewMats[i] = glm::inverse(ovrInterface->toMat4(pose.mDeviceToAbsoluteTracking) * viewMats[i]);
            viewPos[i] = glm::inverse(viewMats[i])[3];
        }


        currentPRP->lateUpdateVP(viewMats, viewPos, *device);

        vr::VRCompositor()->SubmitExplicitTimingData();
    }

    vk::SubmitInfo submit;
    submit.waitSemaphoreCount = 1;
    vk::Semaphore cImageAcquire = *imageAcquire;
    submit.pWaitSemaphores = &cImageAcquire;
    vk::PipelineStageFlags waitStages = vk::PipelineStageFlagBits::eBottomOfPipe;
    submit.pWaitDstStageMask = &waitStages;
    submit.commandBufferCount = 1;
    vk::CommandBuffer cCmdBuf = *cmdBuf;
    submit.pCommandBuffers = &cCmdBuf;

    vk::Semaphore waitSemaphore = *commandComplete;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &waitSemaphore;
    device->getQueue(graphicsQueueFamilyIdx, 0).submit(1, &submit, cmdBufferFences[imageIndex]);
    TracyMessageL("Queue submitted");

    if (enableVR) {
        // Submit to SteamVR
        vr::VRTextureBounds_t bounds;
        bounds.uMin = 0.0f;
        bounds.uMax = 1.0f;
        bounds.vMin = 0.0f;
        bounds.vMax = 1.0f;

        vr::VRVulkanTextureData_t vulkanData;
        VkImage vkImg = rtResources.at(finalPrePresent).image.image();
        vulkanData.m_nImage = (uint64_t)vkImg;
        vulkanData.m_pDevice = (VkDevice_T*)*device;
        vulkanData.m_pPhysicalDevice = (VkPhysicalDevice_T*)physicalDevice;
        vulkanData.m_pInstance = (VkInstance_T*)*instance;
        vulkanData.m_pQueue = (VkQueue_T*)device->getQueue(graphicsQueueFamilyIdx, 0);
        vulkanData.m_nQueueFamilyIndex = graphicsQueueFamilyIdx;

        vulkanData.m_nWidth = renderWidth;
        vulkanData.m_nHeight = renderHeight;
        vulkanData.m_nFormat = VK_FORMAT_R8G8B8A8_UNORM;
        vulkanData.m_nSampleCount = 1;

        // Image submission with validation layers turned on causes a crash
        // If we really want the validation layers, don't submit anything
        if (!vrValidationLayers) {
            vr::Texture_t texture = { &vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Auto };
            vr::VRCompositor()->Submit(vr::Eye_Left, &texture, &bounds);

            vulkanData.m_nImage = (uint64_t)(VkImage)rtResources.at(finalPrePresentR).image.image();
            vr::VRCompositor()->Submit(vr::Eye_Right, &texture, &bounds);
        }
    }

    vk::PresentInfoKHR presentInfo;
    vk::SwapchainKHR cSwapchain = *swapchain->getSwapchain();
    presentInfo.pSwapchains = &cSwapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imageIndex;

    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    vk::Result presentResult = device->getQueue(presentQueueFamilyIdx, 0).presentKHR(presentInfo);

    TracyMessageL("Presented");

    if (vrApi == VrApi::OpenVR)
        vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);

    std::array<std::uint64_t, 2> timeStamps = { {0} };
    device->getQueryPoolResults<std::uint64_t>(
        *queryPool, 0, (uint32_t)timeStamps.size(),
        timeStamps, sizeof(std::uint64_t),
        vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait
        );
    lastRenderTimeTicks = timeStamps[1] - timeStamps[0];
    frameIdx++;
    FrameMark
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
        indices.push_back((uint32_t)indices.size());
    }
}

void VKRenderer::preloadMesh(AssetID id) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    auto ext = g_assetDB.getAssetExtension(id);

    if (ext == ".obj") { // obj
        PhysFS::ifstream meshFileStream(g_assetDB.openAssetFileRead(id));
        loadObj(vertices, indices, meshFileStream);
    } else if (ext == ".mdl") { // source model

    }

    auto memProps = physicalDevice.getMemoryProperties();
    LoadedMeshData lmd;
    lmd.indexType = vk::IndexType::eUint32;
    lmd.indexCount = (uint32_t)indices.size();
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
    procObj.indexCount = (uint32_t)procObj.indices.size();
    procObj.ib = vku::IndexBuffer{ *device, allocator, procObj.indices.size() * sizeof(uint32_t) };
    procObj.ib.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), procObj.indices);
    procObj.vb = vku::VertexBuffer{ *device, allocator, procObj.vertices.size() * sizeof(Vertex) };
    procObj.vb.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), procObj.vertices);
}

bool VKRenderer::getPickedEnt(entt::entity* entOut) {
    return currentPRP->getPickedEnt((uint32_t*)entOut);
}

void VKRenderer::requestEntityPick() {
    return currentPRP->requestEntityPick();
}

void VKRenderer::unloadUnusedMaterials(entt::registry& reg) {
    bool textureReferenced[NUM_TEX_SLOTS];
    bool materialReferenced[NUM_MAT_SLOTS];

    memset(textureReferenced, 0, sizeof(textureReferenced));
    memset(materialReferenced, 0, sizeof(materialReferenced));

    reg.view<WorldObject>().each([&materialReferenced, &textureReferenced, this](entt::entity, WorldObject& wo) {
        materialReferenced[wo.materialIdx] = true;

        uint32_t albedoIdx = (*matSlots)[wo.materialIdx].pack0.z;
        textureReferenced[albedoIdx] = true;
        });

    for (uint32_t i = 0; i < NUM_MAT_SLOTS; i++) {
        if (!materialReferenced[i]) matSlots->unload(i);
    }

    for (uint32_t i = 0; i < NUM_TEX_SLOTS; i++) {
        if (!textureReferenced[i]) texSlots->unload(i);
    }
}

void VKRenderer::reloadMatsAndTextures() {
    device->waitIdle();
    for (uint32_t i = 0; i < NUM_MAT_SLOTS; i++) {
        if (matSlots->isSlotPresent(i))
            matSlots->unload(i);
    }

    for (uint32_t i = 0; i < NUM_TEX_SLOTS; i++) {
        if (texSlots->isSlotPresent(i))
            texSlots->unload(i);
    }

    clearMaterialIndices = true;
}

VKRenderer::~VKRenderer() {
    if (this->device) {
        this->device->waitIdle();
        // Some stuff has to be manually destroyed

        for (auto& fence : this->cmdBufferFences) {
            this->device->destroyFence(fence);
        }

        graphSolver.clear();

        texSlots.reset();
        matSlots.reset();

        rtResources.clear();
        loadedMeshes.clear();
        vmaDestroyAllocator(allocator);

        this->swapchain.reset();

        this->instance->destroySurfaceKHR(this->surface);
    }
}