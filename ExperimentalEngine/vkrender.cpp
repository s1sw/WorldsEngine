#include <vulkan/vulkan.hpp>
#define VMA_IMPLEMENTATION
#include "PCH.hpp"
#include "Engine.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "imgui_impl_vulkan.h"
#include "physfs.hpp"
#include "Transform.hpp"
#include "tracy/Tracy.hpp"
#include "tracy/TracyC.h"
#include "tracy/TracyVulkan.hpp"
#include "RenderPasses.hpp"
#include "Input.hpp"
#include "OpenVRInterface.hpp"
#include <sajson.h>
#include "Fatal.hpp"
#include <unordered_set>
#include "Log.hpp"
#include "ObjModelLoader.hpp"
#include "Render.hpp"
#include "SourceModelLoader.hpp"
#include "WMDLLoader.hpp"

using namespace worlds;

const bool vrValidationLayers = false;

// adapted from https://zeux.io/2019/07/17/serializing-pipeline-cache/
struct PipelineCacheDataHeader {
    PipelineCacheDataHeader() {
        magic[0] = 'W';
        magic[1] = 'P';
        magic[2] = 'L';
        magic[3] = 'C';
    }

    uint8_t magic[4];    // an arbitrary magic header to make sure this is actually our file
    uint32_t dataSize; // equal to *pDataSize returned by vkGetPipelineCacheData

    uint32_t vendorID;      // equal to VkPhysicalDeviceProperties::vendorID
    uint32_t deviceID;      // equal to VkPhysicalDeviceProperties::deviceID
    uint32_t driverVersion; // equal to VkPhysicalDeviceProperties::driverVersion

    uint8_t uuid[VK_UUID_SIZE]; // equal to VkPhysicalDeviceProperties::pipelineCacheUUID
};

std::string getPipelineCachePath(const vk::PhysicalDeviceProperties& physDevProps) {
    char* base_path = SDL_GetPrefPath("Someone Somewhere", "Worlds Engine");
    std::string ret = base_path + std::string((char*)physDevProps.deviceName.data()) + "-pipelinecache.wplc";

    SDL_free(base_path);

    return ret;
}

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
    RenderTextureResource rtr;
    rtr.image = vku::GenericImage{ *device, allocator, resourceCreateInfo.ici, resourceCreateInfo.viewType, resourceCreateInfo.aspectFlags, false, debugName };
    rtr.aspectFlags = resourceCreateInfo.aspectFlags;

    RenderImageHandle handle = lastHandle++;
    rtResources.insert({ handle, std::move(rtr) });
    return handle;
}

void VKRenderer::createSwapchain(vk::SwapchainKHR oldSwapchain) {
    vk::PresentModeKHR presentMode = (useVsync && !enableVR) ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eImmediate;
    QueueFamilyIndices qfi{ graphicsQueueFamilyIdx, presentQueueFamilyIdx };
    swapchain = std::make_unique<Swapchain>(physicalDevice, *device, surface, qfi, oldSwapchain, presentMode);
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
    for (size_t i = 0; i != swapchain->imageViews.size(); i++) {
        vk::ImageView attachments[1] = { swapchain->imageViews[i] };
        vk::FramebufferCreateInfo fci;
        fci.attachmentCount = 1;
        fci.pAttachments = attachments;
        fci.width = width;
        fci.height = height;
        fci.renderPass = irp->getRenderPass();
        fci.layers = 1;
        framebuffers.push_back(device->createFramebufferUnique(fci));
    }
}

void loadPipelineCache(const vk::PhysicalDeviceProperties& physDevProps, vk::PipelineCacheCreateInfo& pcci) {
    std::string pipelineCachePath = getPipelineCachePath(physDevProps);

    FILE* f = fopen(pipelineCachePath.c_str(), "rb");

    if (f) {
        PipelineCacheDataHeader cacheDataHeader;
        size_t readBytes = fread(&cacheDataHeader, 1, sizeof(cacheDataHeader), f);

        if (readBytes != sizeof(cacheDataHeader)) {
            logErr(WELogCategoryRender, "Error while loading pipeline cache: read %i out of %i header bytes", readBytes, sizeof(cacheDataHeader));
            fclose(f);
            return;
        }

        if (cacheDataHeader.deviceID != physDevProps.deviceID ||
            cacheDataHeader.driverVersion != physDevProps.driverVersion ||
            cacheDataHeader.vendorID != physDevProps.vendorID) {
            logErr(WELogCategoryRender, "Error while loading pipeline cache: device properties didn't match");
            fclose(f);
            pcci.pInitialData = nullptr;
            pcci.initialDataSize = 0;
            return;
        }

        pcci.pInitialData = std::malloc(cacheDataHeader.dataSize);
        pcci.initialDataSize = cacheDataHeader.dataSize;
        readBytes = fread((void*)pcci.pInitialData, 1, cacheDataHeader.dataSize, f);

        if (readBytes != cacheDataHeader.dataSize) {
            logErr(WELogCategoryRender, "Error while loading pipeline cache: couldn't read data");
            std::free((void*)pcci.pInitialData);
            pcci.pInitialData = nullptr;
            pcci.initialDataSize = 0;
            fclose(f);
            return;
        }

        logMsg(WELogCategoryRender, "Loaded pipeline cache: %i bytes", cacheDataHeader.dataSize);

        fclose(f);
    }
}

void VKRenderer::createInstance(const RendererInitInfo& initInfo) {
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
        logMsg(WELogCategoryRender, "supported extension: %s", v.extensionName.data());
    }

    for (auto& e : instanceExtensions) {
        logMsg(WELogCategoryRender, "activating extension: %s", e.c_str());
        instanceMaker.extension(e.c_str());
    }

#ifndef NDEBUG
    if (!enableVR || vrValidationLayers) {
        instanceMaker.layer("VK_LAYER_KHRONOS_validation");
        instanceMaker.extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
#endif
    instanceMaker.extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    instanceMaker.applicationName(initInfo.applicationName ? "Worlds Engine" : initInfo.applicationName)
        .engineName("Worlds")
        .applicationVersion(1)
        .engineVersion(1);

    instance = instanceMaker.createUnique();
}

void logPhysDevInfo(const vk::PhysicalDevice& physicalDevice) {
    auto memoryProps = physicalDevice.getMemoryProperties();

    auto physDevProps = physicalDevice.getProperties();
    logMsg(worlds::WELogCategoryRender, "Physical device:\n");
    logMsg(worlds::WELogCategoryRender, "\t-Name: %s", physDevProps.deviceName.data());
    logMsg(worlds::WELogCategoryRender, "\t-ID: %u", physDevProps.deviceID);
    logMsg(worlds::WELogCategoryRender, "\t-Vendor ID: %u", physDevProps.vendorID);
    logMsg(worlds::WELogCategoryRender, "\t-Device Type: %s", vk::to_string(physDevProps.deviceType).c_str());
    logMsg(worlds::WELogCategoryRender, "\t-Driver Version: %u", physDevProps.driverVersion);
    logMsg(worlds::WELogCategoryRender, "\t-Memory heap count: %u", memoryProps.memoryHeapCount);
    logMsg(worlds::WELogCategoryRender, "\t-Memory type count: %u", memoryProps.memoryTypeCount);

    vk::DeviceSize totalVram = 0;
    for (uint32_t i = 0; i < memoryProps.memoryHeapCount; i++) {
        auto& heap = memoryProps.memoryHeaps[i];
        totalVram += heap.size;
        logMsg(worlds::WELogCategoryRender, "Heap %i: %hu MB", i, heap.size / 1024 / 1024);
    }

    for (uint32_t i = 0; i < memoryProps.memoryTypeCount; i++) {
        auto& memType = memoryProps.memoryTypes[i];
        logMsg(worlds::WELogCategoryRender, "Memory type for heap %i: %s", memType.heapIndex, vk::to_string(memType.propertyFlags).c_str());
    }

    logMsg(worlds::WELogCategoryRender, "Approx. %hu MB total accessible graphics memory (NOT VRAM!)", totalVram / 1024 / 1024);
}

bool checkPhysicalDeviceFeatures(const vk::PhysicalDevice& physDev) {
    vk::PhysicalDeviceFeatures supportedFeatures = physDev.getFeatures();
    if (!supportedFeatures.shaderStorageImageMultisample) {
        logWarn(worlds::WELogCategoryRender, "Missing shaderStorageImageMultisample");
        return false;
    }

    if (!supportedFeatures.fragmentStoresAndAtomics) {
        logWarn(worlds::WELogCategoryRender, "Missing fragmentStoresAndAtomics");
    }

    if (!supportedFeatures.fillModeNonSolid) {
        logWarn(worlds::WELogCategoryRender, "Missing fillModeNonSolid");
        return false;
    }

    if (!supportedFeatures.wideLines) {
        logWarn(worlds::WELogCategoryRender, "Missing wideLines");
        return false;
    }

    return true;
}

bool isDeviceBetter(vk::PhysicalDevice a, vk::PhysicalDevice b) {
    auto aProps = a.getProperties();
    auto bProps = b.getProperties();

    if (bProps.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && aProps.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
        return true;
    }

    return aProps.deviceID < bProps.deviceID;
}

vk::PhysicalDevice pickPhysicalDevice(std::vector<vk::PhysicalDevice>& physicalDevices) {
    std::sort(physicalDevices.begin(), physicalDevices.end(), isDeviceBetter);

    return physicalDevices[0];
}

VKRenderer::VKRenderer(const RendererInitInfo& initInfo, bool* success)
    : finalPrePresent(UINT_MAX)
    , finalPrePresentR(UINT_MAX)
    , shadowmapImage(std::numeric_limits<uint32_t>::max())
    , imguiImage(UINT_MAX)
    , window(initInfo.window)
    , lastHandle(0)
    , frameIdx(0)
    , shadowmapRes(4096)
    , enableVR(initInfo.enableVR)
    , pickingPRP(nullptr)
    , vrPRP(nullptr)
    , irp(nullptr)
    , vrPredictAmount(0.033f)
    , clearMaterialIndices(false)
    , useVsync(true) 
    , lowLatencyMode("r_lowLatency", "0", "Waits for GPU completion before starting the next frame. Has a significant impact on latency when VSync is enabled.")
    , enablePicking(initInfo.enablePicking)
    , nextHandle(0u) {
    msaaSamples = vk::SampleCountFlagBits::e2;
    numMSAASamples = 2;

    createInstance(initInfo);

#ifndef NDEBUG
    if (!enableVR || vrValidationLayers)
        dbgCallback = vku::DebugCallback(*instance);
#endif
    auto physDevs = instance->enumeratePhysicalDevices();
    physicalDevice = pickPhysicalDevice(physDevs);

    logPhysDevInfo(physicalDevice);

    auto qprops = physicalDevice.getQueueFamilyProperties();
    const auto badQueue = ~(uint32_t)0;
    graphicsQueueFamilyIdx = badQueue;
    computeQueueFamilyIdx = badQueue;
    vk::QueueFlags search = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;

    // Look for a queue family with both graphics and
    // compute first.
    for (uint32_t qi = 0; qi != qprops.size(); ++qi) {
        auto& qprop = qprops[qi];
        if ((qprop.queueFlags & search) == search) {
            graphicsQueueFamilyIdx = qi;
            computeQueueFamilyIdx = qi;
            break;
        }
    }

    // Search for async compute queue family
    asyncComputeQueueFamilyIdx = badQueue;
    for (size_t i = 0; i < qprops.size(); i++) {
        auto& qprop = qprops[i];
        if ((qprop.queueFlags & (vk::QueueFlagBits::eCompute)) == vk::QueueFlagBits::eCompute && i != computeQueueFamilyIdx) {
            asyncComputeQueueFamilyIdx = i;
            break;
        }
    }

    if (asyncComputeQueueFamilyIdx == badQueue)
        logWarn(worlds::WELogCategoryRender, "Couldn't find async compute queue");

    if (graphicsQueueFamilyIdx == badQueue || computeQueueFamilyIdx == badQueue) {
        *success = false;
        return;
    }

    vku::DeviceMaker dm{};
    dm.defaultLayers();
    dm.queue(graphicsQueueFamilyIdx);

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

    if (!checkPhysicalDeviceFeatures(physicalDevice)) {
        *success = false;
        return;
    }

    vk::PhysicalDeviceFeatures features;
    features.shaderStorageImageMultisample = true;
    features.fragmentStoresAndAtomics = true;
    features.fillModeNonSolid = true;
    features.wideLines = true;
    features.samplerAnisotropy = true;
    dm.setFeatures(features);

    vk::PhysicalDeviceVulkan12Features vk12Features;
    vk12Features.timelineSemaphore = true;
    vk12Features.descriptorBindingPartiallyBound = true;
    vk12Features.runtimeDescriptorArray = true;
    dm.setPNext(&vk12Features);

    if (computeQueueFamilyIdx != graphicsQueueFamilyIdx) dm.queue(computeQueueFamilyIdx);
    device = dm.createUnique(physicalDevice);

    VmaAllocatorCreateInfo allocatorCreateInfo;
    memset(&allocatorCreateInfo, 0, sizeof(allocatorCreateInfo));
    allocatorCreateInfo.device = *device;
    allocatorCreateInfo.frameInUseCount = 0;
    allocatorCreateInfo.instance = *instance;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorCreateInfo.flags = 0;
    vmaCreateAllocator(&allocatorCreateInfo, &allocator);

    vk::PipelineCacheCreateInfo pipelineCacheInfo{};
    loadPipelineCache(physicalDevice.getProperties(), pipelineCacheInfo);
    pipelineCache = device->createPipelineCacheUnique(pipelineCacheInfo);
    std::free((void*)pipelineCacheInfo.pInitialData);

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
    descriptorPool = device->createDescriptorPoolUnique(descriptorPoolInfo);

    // Create surface and find presentation queue
    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, *instance, &surface);

    this->surface = surface;
    presentQueueFamilyIdx = findPresentQueue(physicalDevice, surface);

    int qfi = 0;
    for (auto& qprop : qprops) {
        logMsg(worlds::WELogCategoryRender, "Queue family with properties %s (supports present: %i)", vk::to_string(qprop.queueFlags).c_str(), physicalDevice.getSurfaceSupportKHR(qfi, surface));
        qfi++;
    }

    // Semaphores for presentation
    vk::SemaphoreCreateInfo sci;
    imageAcquire = device->createSemaphoreUnique(sci);
    commandComplete = device->createSemaphoreUnique(sci);

    // Command pool
    vk::CommandPoolCreateInfo cpci;
    cpci.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    cpci.queueFamilyIndex = graphicsQueueFamilyIdx;
    commandPool = device->createCommandPoolUnique(cpci);

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
    cubemapSlots = std::make_unique<CubemapSlots>(vkCtx);

    vk::ImageCreateInfo brdfLutIci { 
        vk::ImageCreateFlags{}, 
        vk::ImageType::e2D, 
        vk::Format::eR16G16Sfloat, 
        vk::Extent3D{256,256,1}, 1, 1, 
        vk::SampleCountFlagBits::e1, 
        vk::ImageTiling::eOptimal, 
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::SharingMode::eExclusive, graphicsQueueFamilyIdx 
    };

    brdfLut = vku::GenericImage{ *device, allocator, brdfLutIci, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, false, "BRDF LUT" };

    cubemapConvoluter = std::make_unique<CubemapConvoluter>(vkCtx);

    vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [&](auto cb) {
        brdfLut.setLayout(cb, vk::ImageLayout::eColorAttachmentOptimal);
    });

    BRDFLUTRenderer brdfLutRenderer{ *vkCtx };
    brdfLutRenderer.render(*vkCtx, brdfLut);

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

    createSCDependents();

    vk::CommandBufferAllocateInfo cbai;
    cbai.commandPool = *commandPool;
    cbai.commandBufferCount = 4;
    cbai.level = vk::CommandBufferLevel::ePrimary;
    cmdBufs = device->allocateCommandBuffersUnique(cbai);

    for (size_t i = 0; i < cmdBufs.size(); i++) {
        vk::FenceCreateInfo fci;
        fci.flags = vk::FenceCreateFlagBits::eSignaled;
        vk::SemaphoreCreateInfo sci;
        vk::SemaphoreTypeCreateInfo stci;
        stci.initialValue = 0;
        stci.semaphoreType = vk::SemaphoreType::eTimeline;
        sci.pNext = &stci;
        cmdBufferSemaphores.push_back(device->createSemaphore(sci));
        cmdBufSemaphoreVals.push_back(0);

        vk::CommandBuffer cb = *cmdBufs[i];
        vk::CommandBufferBeginInfo cbbi;
        cb.begin(cbbi);
        cb.end();
    }

    timestampPeriod = physicalDevice.getProperties().limits.timestampPeriod;

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
        if (initInfo.activeVrApi == VrApi::OpenVR) {
            vr::VRCompositor()->SetExplicitTimingMode(vr::EVRCompositorTimingMode::VRCompositorTimingMode_Explicit_RuntimePerformsPostPresentHandoff);
        }

        vrInterface = initInfo.vrInterface;
        vrApi = initInfo.activeVrApi;
    }

    uint32_t s = cubemapSlots->loadOrGet(g_assetDB.addOrGetExisting("Cubemap2.json"));
    cubemapConvoluter->convolute((*cubemapSlots)[s]);

    g_console->registerCommand([&](void*, const char* arg) {
        numMSAASamples = std::atoi(arg);
        // The sample count flags are actually identical to the number of samples
        msaaSamples = (vk::SampleCountFlagBits)numMSAASamples;
        recreateSwapchain();
        }, "r_setMSAASamples", "Sets the number of MSAA samples.", nullptr);

    g_console->registerCommand([&](void*, const char*) {
        recreateSwapchain();
        }, "r_recreateSwapchain", "", nullptr);

    g_console->registerCommand([&](void*, const char*) {
        char* statsString;
        vmaBuildStatsString(allocator, &statsString, true);
        logMsg("%s", statsString);
        vmaFreeStatsString(allocator, statsString);
        }, "r_printAllocInfo", "", nullptr);
}

// Quite a lot of resources are dependent on either the number of images
// there are in the swap chain or the swapchain itself, so they need to be
// recreated whenever the swap chain changes.
void VKRenderer::createSCDependents() {
    if (rtResources.count(imguiImage) != 0) {
        rtResources.erase(imguiImage);
    }

    if (rtResources.count(finalPrePresent) != 0) {
        rtResources.erase(finalPrePresent);
    }

    if (rtResources.count(finalPrePresentR) != 0) {
        rtResources.erase(finalPrePresentR);
    }

    PassSetupCtx psc{ 
        getVKCtx(),
        &texSlots, 
        &cubemapSlots, 
        &matSlots, 
        rtResources, 
        (int)swapchain->images.size(), 
        enableVR, 
        &brdfLut 
    };

    if (irp == nullptr) {
        irp = new ImGuiRenderPass(*swapchain);
        irp->setup(psc);
    }

    createFramebuffers();
    irp->handleResize(psc, finalPrePresent);

    vk::ImageCreateInfo ici;
    ici.imageType = vk::ImageType::e2D;
    ici.extent = vk::Extent3D{ renderWidth, renderHeight, 1 };
    ici.arrayLayers = 1;
    ici.mipLevels = 1;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    ici.format = vk::Format::eR8G8B8A8Unorm;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | 
                vk::ImageUsageFlagBits::eSampled | 
                vk::ImageUsageFlagBits::eStorage;

    RTResourceCreateInfo imguiImageCreateInfo{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };
    imguiImage = createRTResource(imguiImageCreateInfo, "ImGui Image");

    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | 
                vk::ImageUsageFlagBits::eSampled | 
                vk::ImageUsageFlagBits::eStorage | 
                vk::ImageUsageFlagBits::eTransferSrc;

    RTResourceCreateInfo finalPrePresentCI{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };

    finalPrePresent = createRTResource(finalPrePresentCI, "Final Pre-Present");

    if (enableVR)
        finalPrePresentR = createRTResource(finalPrePresentCI, "Final Pre-Present R");

    vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [&](vk::CommandBuffer cmdBuf) {
        rtResources.at(finalPrePresent).image.setLayout(cmdBuf, vk::ImageLayout::eTransferSrcOptimal);
        });

    RTTPassHandle screenPass = ~0u;
    for (auto& p : rttPasses) {
        if (p.second.outputToScreen) {
            screenPass = p.first;
        }
    }

    if (screenPass != ~0u) {
        if (rttPasses.at(screenPass).isVr) {
            vrPRP = nullptr;
        }
        destroyRTTPass(screenPass);
    }
}

void VKRenderer::recreateSwapchain() {
    // Wait for current frame to finish
    device->waitIdle();

    // Check width/height - if it's 0, just ignore it
    auto surfaceCaps = physicalDevice.getSurfaceCapabilitiesKHR(surface);

    if (surfaceCaps.currentExtent.width == 0 || surfaceCaps.currentExtent.height == 0) {
        logMsg(WELogCategoryRender, "Ignoring resize with 0 width or height");
        isMinimised = true;

        while (isMinimised) {
            auto surfaceCaps = physicalDevice.getSurfaceCapabilitiesKHR(surface);
            isMinimised = surfaceCaps.currentExtent.width == 0 || surfaceCaps.currentExtent.height == 0;
            SDL_PumpEvents();
            SDL_Delay(50);
        }

        recreateSwapchain();
        return;
    }

    isMinimised = false;

    logMsg(WELogCategoryRender, "Recreating swapchain: New surface size is %ix%i",
        surfaceCaps.currentExtent.width, surfaceCaps.currentExtent.height);

    if (surfaceCaps.currentExtent.width > 0 && surfaceCaps.currentExtent.height > 0) {
        width = surfaceCaps.currentExtent.width;
        height = surfaceCaps.currentExtent.height;
    }

    if (!enableVR) {
        renderWidth = width;
        renderHeight = height;
    }

    if (surfaceCaps.currentExtent.width == 0 || surfaceCaps.currentExtent.height == 0) {
        isMinimised = true;
        return;
    } else {
        isMinimised = false;
    }

    std::unique_ptr<Swapchain> oldSwapchain = std::move(swapchain);

    createSwapchain(*oldSwapchain->getSwapchain());

    framebuffers.clear();
    oldSwapchain.reset();
    imageAcquire.reset();
    vk::SemaphoreCreateInfo sci;
    imageAcquire = device->createSemaphoreUnique(sci);

    createSCDependents();

    swapchainRecreated = true;
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
    device->getQueue(presentQueueFamilyIdx, 0).submit(submitInfo, nullptr);

    auto presentResult = device->getQueue(presentQueueFamilyIdx, 0).presentKHR(presentInfo);
    if (presentResult != vk::Result::eSuccess && presentResult != vk::Result::eSuboptimalKHR)
        fatalErr("Present failed!");
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
    ZoneScoped;
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

uint32_t nextImageIdx = 0;
bool firstFrame = true;

void VKRenderer::acquireSwapchainImage(uint32_t* imageIdx) {
    ZoneScoped;
    vk::Result nextImageRes = swapchain->acquireImage(*device, *imageAcquire, imageIdx);

    if ((nextImageRes == vk::Result::eSuboptimalKHR || nextImageRes == vk::Result::eErrorOutOfDateKHR) && width != 0 && height != 0) {
        recreateSwapchain();

        // acquire image from new swapchain
        swapchain->acquireImage(*device, *imageAcquire, imageIdx);
    }
}

void VKRenderer::submitToOpenVR() {
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

void VKRenderer::uploadSceneAssets(entt::registry& reg, RenderCtx& rCtx) {
    ZoneScoped;
    // Upload any necessary materials + meshes
    reg.view<WorldObject>().each([this, &rCtx](auto, WorldObject& wo) {
        for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
            if (!wo.presentMaterials[i]) continue;

            if (wo.materialIdx[i] == ~0u) {
                rCtx.reuploadMats = true;
                wo.materialIdx[i] = matSlots->loadOrGet(wo.materials[i]);
            }
        }

        if (loadedMeshes.find(wo.mesh) == loadedMeshes.end()) {
            preloadMesh(wo.mesh);
        }
        });

    reg.view<ProceduralObject>().each([this, &rCtx](auto, ProceduralObject& po) {
        if (po.materialIdx == ~0u) {
            rCtx.reuploadMats = true;
            po.materialIdx = matSlots->loadOrGet(po.material);
        }
        });

    reg.view<WorldCubemap>().each([&](auto, WorldCubemap& wc) {
        if (wc.loadIdx == ~0u) {
            wc.loadIdx = cubemapSlots->loadOrGet(wc.cubemapId);
            cubemapConvoluter->convolute(cubemapSlots->getSlots()[wc.loadIdx]);
            rCtx.reuploadMats = true;
        }
    });
}

bool lowLatencyLast = false;

void VKRenderer::writeCmdBuf(vk::UniqueCommandBuffer& cmdBuf, uint32_t imageIndex, Camera& cam, entt::registry& reg) {
    ZoneScoped;
    std::unordered_map<RenderImageHandle, vk::ImageAspectFlagBits> rtAspects;

    for (auto& pair : rtResources) {
        rtAspects.insert({ pair.first, pair.second.aspectFlags });
    }

    RenderCtx rCtx{ cmdBuf, reg, imageIndex, cam, rtResources, renderWidth, renderHeight, loadedMeshes };
    rCtx.enableVR = enableVR;
    rCtx.materialSlots = &matSlots;
    rCtx.textureSlots = &texSlots;
    rCtx.cubemapSlots = &cubemapSlots;
    rCtx.viewPos = cam.position;
    rCtx.dbgStats = &dbgStats;

#ifdef TRACY_ENABLE
    rCtx.tracyContexts = &tracyContexts;
#endif

    if (enableVR) {
        if (vrApi == VrApi::OpenVR) {
            OpenVRInterface* ovrInterface = static_cast<OpenVRInterface*>(vrInterface);
            rCtx.vrProjMats[0] = ovrInterface->getProjMat(vr::EVREye::Eye_Left, 0.01f, 100.0f);
            rCtx.vrProjMats[1] = ovrInterface->getProjMat(vr::EVREye::Eye_Right, 0.01f, 100.0f);

            vr::TrackedDevicePose_t pose;
            vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::ETrackingUniverseOrigin::TrackingUniverseStanding, vrPredictAmount, &pose, 1);

            glm::mat4 viewMats[2];
            rCtx.vrViewMats[0] = ovrInterface->getViewMat(vr::EVREye::Eye_Left);
            rCtx.vrViewMats[1] = ovrInterface->getViewMat(vr::EVREye::Eye_Right);

            for (int i = 0; i < 2; i++) {
                rCtx.vrViewMats[i] = glm::inverse(ovrInterface->toMat4(pose.mDeviceToAbsoluteTracking) * viewMats[i]) * cam.getViewMatrix();
            }
        }
    }

    vk::CommandBufferBeginInfo cbbi;
    cbbi.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    cmdBuf->begin(cbbi);
    cmdBuf->resetQueryPool(*queryPool, 0, 2);
    cmdBuf->writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0);

    texSlots->setUploadCommandBuffer(*cmdBuf, imageIndex);

    if (clearMaterialIndices) {
        reg.view<WorldObject>().each([](entt::entity, WorldObject& wo) {
            memset(wo.materialIdx, ~0u, sizeof(wo.materialIdx));
            });
        clearMaterialIndices = false;
    }

    PassSetupCtx psc{ getVKCtx(), &texSlots, &cubemapSlots, &matSlots, rtResources, (int)swapchain->images.size(), enableVR, &brdfLut };

    uploadSceneAssets(reg, rCtx);

    vku::transitionLayout(*cmdBuf, rtResources.at(finalPrePresent).image.image(),
        vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eColorAttachmentWrite);

    int numActivePasses = 0;
    for (auto& p : rttPasses) {
        if (!p.second.active) continue;
        numActivePasses++;
        std::vector<RenderPass*> solvedNodes = p.second.graphSolver.solve();
        std::vector<std::vector<ImageBarrier>> barriers = p.second.graphSolver.createImageBarriers(solvedNodes, rtAspects);

        if (!p.second.outputToScreen) {
            vku::transitionLayout(*cmdBuf, rtResources.at(p.second.sdrFinalTarget).image.image(),
                vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eColorAttachmentOptimal,
                vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eColorAttachmentWrite);
        }

        auto& rpi = p.second;
        rCtx.width = rpi.width;
        rCtx.height = rpi.height;

        for (auto& node : solvedNodes) {
            node->prePass(psc, rCtx);
        }

        for (size_t i = 0; i < solvedNodes.size(); i++) {
            auto& node = solvedNodes[i];
            // Put in barriers for this node
            for (auto& barrier : barriers[i])
                imageBarrier(*cmdBuf, barrier);

            node->execute(rCtx);
        }

        if (!p.second.outputToScreen) {
            vku::transitionLayout(*cmdBuf, rtResources.at(p.second.sdrFinalTarget).image.image(),
                vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
                vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead);
        }
    }
    dbgStats.numRTTPasses = numActivePasses;
    rCtx.width = width;
    rCtx.height = height;

    vku::transitionLayout(*cmdBuf, swapchain->images[imageIndex],
        vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer,
        vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite);

    vku::transitionLayout(*cmdBuf, rtResources.at(finalPrePresent).image.image(),
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead);

    if (enableVR) {
        vku::transitionLayout(*cmdBuf, rtResources.at(finalPrePresentR).image.image(),
            vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead);
    }

    cmdBuf->clearColorImage(swapchain->images[imageIndex], vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue{ std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } }, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    if (!enableVR) {
        vk::ImageBlit imageBlit;
        imageBlit.srcOffsets[1] = imageBlit.dstOffsets[1] = vk::Offset3D{ (int)width, (int)height, 1 };
        imageBlit.dstSubresource = imageBlit.srcSubresource = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
        cmdBuf->blitImage(
            rtResources.at(finalPrePresent).image.image(), vk::ImageLayout::eTransferSrcOptimal,
            swapchain->images[imageIndex], vk::ImageLayout::eTransferDstOptimal,
            imageBlit, vk::Filter::eNearest);
    } else {
        // Calculate the best crop for the current window size against the VR render target
        float scaleFac = glm::min((float)windowSize.x / renderWidth, (float)windowSize.y / renderHeight);

        vk::ImageBlit imageBlit;
        imageBlit.srcOffsets[1] = vk::Offset3D{ (int32_t)renderWidth, (int32_t)renderHeight, 1 };
        imageBlit.dstOffsets[1] = vk::Offset3D{ (int32_t)(renderWidth * scaleFac), (int32_t)(renderHeight * scaleFac), 1 };
        imageBlit.dstSubresource = imageBlit.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };

        cmdBuf->blitImage(
            rtResources.at(finalPrePresentR).image.image(), vk::ImageLayout::eTransferSrcOptimal,
            swapchain->images[imageIndex], vk::ImageLayout::eTransferDstOptimal,
            imageBlit, vk::Filter::eNearest);
    }

    vku::transitionLayout(*cmdBuf, swapchain->images[imageIndex],
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead);
    irp->execute(rCtx, *framebuffers[imageIndex]);

    ::imageBarrier(*cmdBuf, swapchain->images[imageIndex], vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead, vk::AccessFlagBits::eMemoryRead,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe);

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
            viewMats[i] = glm::inverse(ovrInterface->toMat4(pose.mDeviceToAbsoluteTracking) * viewMats[i]) * cam.getViewMatrix();
            viewPos[i] = glm::inverse(viewMats[i])[3];
        }

        if (vrPRP)
            vrPRP->lateUpdateVP(viewMats, viewPos, *device);

        vr::VRCompositor()->SubmitExplicitTimingData();
    }
}

void VKRenderer::frame(Camera& cam, entt::registry& reg) {
    ZoneScoped;
    dbgStats.numCulledObjs = 0;
    dbgStats.numDrawCalls = 0;

    uint32_t imageIndex = nextImageIdx;

    if (!lowLatencyMode.getInt() || !lowLatencyLast) {
        vk::SemaphoreWaitInfo swi;
        swi.pSemaphores = &cmdBufferSemaphores[imageIndex];
        swi.semaphoreCount = 1;
        swi.pValues = &cmdBufSemaphoreVals[imageIndex];
        vk::Result semaphoreWaitResult = device->waitSemaphores(swi, UINT64_MAX);
        if (semaphoreWaitResult != vk::Result::eSuccess)
            fatalErr("failed to wait on semaphore");
    }

    destroyTempTexBuffers(imageIndex);

    if (!lowLatencyMode.getInt() || (!lowLatencyLast && lowLatencyMode.getInt())) {
        if (!(lowLatencyLast && !lowLatencyMode.getInt()))
            acquireSwapchainImage(&imageIndex);
        lowLatencyLast = false;
    }

    if (swapchainRecreated) {
        if (lowLatencyMode.getInt())
            acquireSwapchainImage(&nextImageIdx);
        swapchainRecreated = false;
    }

    auto& cmdBuf = cmdBufs[imageIndex];

    writeCmdBuf(cmdBuf, imageIndex, cam, reg);

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
    submit.signalSemaphoreCount = 2;
    vk::Semaphore signalSemaphores[] = { waitSemaphore, cmdBufferSemaphores[imageIndex] };
    submit.pSignalSemaphores = signalSemaphores;

    vk::TimelineSemaphoreSubmitInfo tssi{};
    uint64_t semaphoreVals[] = { 0, cmdBufSemaphoreVals[imageIndex] + 1 };
    tssi.pSignalSemaphoreValues = semaphoreVals;
    tssi.signalSemaphoreValueCount = 2;

    submit.pNext = &tssi;
    auto queue = device->getQueue(graphicsQueueFamilyIdx, 0);
    auto submitResult = queue.submit(1, &submit, nullptr);

    if (submitResult != vk::Result::eSuccess)
        fatalErr("Failed to submit queue");

    cmdBufSemaphoreVals[imageIndex]++;
    TracyMessageL("Queue submitted");

    if (enableVR) {
        submitToOpenVR();
    }

    vk::PresentInfoKHR presentInfo;
    vk::SwapchainKHR cSwapchain = *swapchain->getSwapchain();
    presentInfo.pSwapchains = &cSwapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imageIndex;

    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    try {
        vk::Result presentResult = queue.presentKHR(presentInfo);

        if (presentResult == vk::Result::eSuboptimalKHR) {
            recreateSwapchain();
        } else if (presentResult != vk::Result::eSuccess) {
            fatalErr("Failed to present");
        }
    } catch (vk::OutOfDateKHRError) {
        recreateSwapchain();
    }

    TracyMessageL("Presented");

    if (vrApi == VrApi::OpenVR)
        vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);

    std::array<std::uint64_t, 2> timeStamps = { {0} };

    auto queryRes = device->getQueryPoolResults(
        *queryPool, 0, (uint32_t)timeStamps.size(), 
        timeStamps.size() * sizeof(uint64_t), timeStamps.data(), 
        sizeof(uint64_t), vk::QueryResultFlagBits::e64
    );

    if (queryRes == vk::Result::eSuccess)
        lastRenderTimeTicks = timeStamps[1] - timeStamps[0];

    if (lowLatencyMode.getInt()) {
        vk::SemaphoreWaitInfo swi;
        swi.pSemaphores = &cmdBufferSemaphores[imageIndex];
        swi.semaphoreCount = 1;
        swi.pValues = &cmdBufSemaphoreVals[imageIndex];
        auto waitResult = device->waitSemaphores(swi, UINT64_MAX);

        if (waitResult != vk::Result::eSuccess)
            fatalErr("Failed to wait on semaphore");

        acquireSwapchainImage(&nextImageIdx);
        lowLatencyLast = true;
    }

    //VmaBudget budget;
    //vmaGetBudget(allocator, &budget);
    //dbgStats.vramUsage = budget.allocationBytes;

    frameIdx++;
    FrameMark;
}

void VKRenderer::preloadMesh(AssetID id) {
    ZoneScoped;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    auto ext = g_assetDB.getAssetExtension(id);

    LoadedMeshData lmd;

    if (ext == ".obj") { // obj
        // Use C++ physfs ifstream for tinyobjloader
        PhysFS::ifstream meshFileStream(g_assetDB.openAssetFileRead(id));
        loadObj(vertices, indices, meshFileStream, lmd);
        lmd.numSubmeshes = 1;
        lmd.submeshes[0].indexCount = indices.size();
        lmd.submeshes[0].indexOffset = 0;
    } else if (ext == ".mdl") { // source model
        std::filesystem::path mdlPath = g_assetDB.getAssetPath(id);
        std::string vtxPath = mdlPath.parent_path().string() + "/" + mdlPath.stem().string();
        vtxPath += ".dx90.vtx";
        std::string vvdPath = mdlPath.parent_path().string() + "/" + mdlPath.stem().string();
        vvdPath += ".vvd";

        AssetID vtxId = g_assetDB.addOrGetExisting(vtxPath);
        AssetID vvdId = g_assetDB.addOrGetExisting(vvdPath);
        loadSourceModel(id, vtxId, vvdId, vertices, indices, lmd);
    } else if (ext == ".wmdl") {
        loadWorldsModel(id, vertices, indices, lmd);
    }

    auto memProps = physicalDevice.getMemoryProperties();
    lmd.indexType = vk::IndexType::eUint32;
    lmd.indexCount = (uint32_t)indices.size();
    lmd.ib = vku::IndexBuffer{ *device, allocator, indices.size() * sizeof(uint32_t), "Mesh Index Buffer" };
    lmd.ib.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), indices);
    lmd.vb = vku::VertexBuffer{ *device, allocator, vertices.size() * sizeof(Vertex), "Mesh Vertex Buffer" };
    lmd.vb.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), vertices);

    lmd.aabbMax = glm::vec3(0.0f);
    lmd.aabbMin = glm::vec3(std::numeric_limits<float>::max());
    lmd.sphereRadius = 0.0f;
    for (auto& vtx : vertices) {
        lmd.sphereRadius = std::max(glm::length(vtx.position), lmd.sphereRadius);
        lmd.aabbMax = glm::max(lmd.aabbMax, vtx.position);
        lmd.aabbMin = glm::min(lmd.aabbMin, vtx.position);
    }

    logMsg(WELogCategoryRender, "Loaded mesh %u, %u verts. Sphere radius %f", id, (uint32_t)vertices.size(), lmd.sphereRadius);

    loadedMeshes.insert({ id, std::move(lmd) });
}

void VKRenderer::uploadProcObj(ProceduralObject& procObj) {
    if (procObj.vertices.size() == 0 || procObj.indices.size() == 0) {
        procObj.visible = false;
        return;
    } else {
        procObj.visible = true;
    }

    device->waitIdle();
    auto memProps = physicalDevice.getMemoryProperties();
    procObj.indexType = vk::IndexType::eUint32;
    procObj.indexCount = (uint32_t)procObj.indices.size();
    procObj.ib = vku::IndexBuffer{ *device, allocator, procObj.indices.size() * sizeof(uint32_t), procObj.dbgName.c_str() };
    procObj.ib.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), procObj.indices);
    procObj.vb = vku::VertexBuffer{ *device, allocator, procObj.vertices.size() * sizeof(Vertex), procObj.dbgName.c_str() };
    procObj.vb.upload(*device, memProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), procObj.vertices);
}

bool VKRenderer::getPickedEnt(entt::entity* entOut) {
    if (pickingPRP)
        return pickingPRP->getPickedEnt((uint32_t*)entOut);
    else
        return false;
}

void VKRenderer::requestEntityPick(int x, int y) {
    if (pickingPRP) {
        pickingPRP->setPickCoords(x, y);
        pickingPRP->requestEntityPick();
    }
}

void VKRenderer::unloadUnusedMaterials(entt::registry& reg) {
    bool textureReferenced[NUM_TEX_SLOTS];
    bool materialReferenced[NUM_MAT_SLOTS];

    memset(textureReferenced, 0, sizeof(textureReferenced));
    memset(materialReferenced, 0, sizeof(materialReferenced));

    reg.view<WorldObject>().each([&materialReferenced, &textureReferenced, this](entt::entity, WorldObject& wo) {
        for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
            if (!wo.presentMaterials[i]) continue;
            materialReferenced[wo.materialIdx[i]] = true;

            uint32_t albedoIdx = (uint32_t)((*matSlots)[wo.materialIdx[i]].albedoTexIdx);
            textureReferenced[albedoIdx] = true;

            int normalTex = (*matSlots)[wo.materialIdx[i]].normalTexIdx;

            if (normalTex > -1) {
                textureReferenced[normalTex] = true;
            }

            int heightmapTex = (*matSlots)[wo.materialIdx[i]].heightmapTexIdx;

            if (heightmapTex > -1) {
                textureReferenced[heightmapTex] = true;
            }
        }
        });

    for (uint32_t i = 0; i < NUM_MAT_SLOTS; i++) {
        if (!materialReferenced[i] && matSlots->isSlotPresent(i)) matSlots->unload(i);
    }

    for (uint32_t i = 0; i < NUM_TEX_SLOTS; i++) {
        if (!textureReferenced[i] && texSlots->isSlotPresent(i)) texSlots->unload(i);
    }

    std::unordered_set<AssetID> referencedMeshes;

    reg.view<WorldObject>().each([&referencedMeshes](entt::entity, WorldObject& wo) {
        referencedMeshes.insert(wo.mesh);
        });

    std::vector<AssetID> toUnload;

    for (auto& p : loadedMeshes) {
        if (!referencedMeshes.contains(p.first))
            toUnload.push_back(p.first);
    }

    for (auto& id : toUnload) {
        loadedMeshes.erase(id);
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

    loadedMeshes.clear();
}

VulkanCtx VKRenderer::getVKCtx() {
    return VulkanCtx{
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
    };
}

RTTPassHandle VKRenderer::createRTTPass(RTTPassCreateInfo& ci) {
    RTTPassInternal rpi;

    vk::ImageCreateInfo ici;
    ici.imageType = vk::ImageType::e2D;
    ici.extent = vk::Extent3D{ ci.width, ci.height, 1 };
    ici.arrayLayers = ci.isVr ? 2 : 1;
    ici.mipLevels = 1;
    ici.format = vk::Format::eR16G16B16A16Sfloat;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    ici.samples = msaaSamples;
    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;

    RTResourceCreateInfo polyCreateInfo{ ici, ci.isVr ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };
    rpi.hdrTarget = createRTResource(polyCreateInfo, "HDR Target");

    ici.format = vk::Format::eD32Sfloat;
    ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    RTResourceCreateInfo depthCreateInfo{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eDepth };
    rpi.depthTarget = createRTResource(depthCreateInfo, "Depth Stencil Image");

    if (ci.enableShadows) {
        auto srp = new ShadowmapRenderPass(shadowmapImage);
        rpi.graphSolver.addNode(srp);
    }

    {
        auto prp = new PolyRenderPass(rpi.depthTarget, rpi.hdrTarget, shadowmapImage, enablePicking);
        if (ci.useForPicking)
            pickingPRP = prp;
        if (ci.isVr)
            vrPRP = prp;
        rpi.graphSolver.addNode(prp);
    }

    ici.arrayLayers = 1;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.format = vk::Format::eR8G8B8A8Unorm;
    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;

    if (!ci.outputToScreen) {
        RTResourceCreateInfo sdrTarget{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };
        rpi.sdrFinalTarget = createRTResource(sdrTarget, "SDR Target");
    }

    PassSetupCtx psc{ getVKCtx(), &texSlots, &cubemapSlots, &matSlots, rtResources, (int)swapchain->images.size(), ci.isVr, &brdfLut };

    auto tonemapRP = new TonemapRenderPass(rpi.hdrTarget, ci.outputToScreen ? finalPrePresent : rpi.sdrFinalTarget);
    rpi.graphSolver.addNode(tonemapRP);

    vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [&](vk::CommandBuffer cmdBuf) {
        rtResources.at(rpi.hdrTarget).image.setLayout(cmdBuf, vk::ImageLayout::eGeneral);
        if (!ci.outputToScreen)
            rtResources.at(rpi.sdrFinalTarget).image.setLayout(cmdBuf, vk::ImageLayout::eShaderReadOnlyOptimal);
        if (enableVR) {
            rtResources.at(finalPrePresentR).image.setLayout(cmdBuf, vk::ImageLayout::eTransferSrcOptimal);
        }
        });

    auto solved = rpi.graphSolver.solve();

    for (auto& node : solved) {
        node->setup(psc);
    }

    if (enableVR) {
        tonemapRP->setRightFinalImage(psc, finalPrePresentR);
    }

    rpi.isVr = ci.isVr;
    rpi.enableShadows = ci.enableShadows;
    rpi.outputToScreen = ci.outputToScreen;
    rpi.width = ci.width;
    rpi.height = ci.height;
    rpi.active = true;

    RTTPassHandle handle = nextHandle;
    nextHandle++;
    rttPasses.insert({ handle, rpi });
    return handle;
}

void VKRenderer::destroyRTTPass(RTTPassHandle handle) {
    device->waitIdle();
    auto& rpi = rttPasses.at(handle);
    rpi.graphSolver.clear();

    rtResources.erase(rpi.hdrTarget);
    rtResources.erase(rpi.depthTarget);

    if (!rpi.outputToScreen)
        rtResources.erase(rpi.sdrFinalTarget);

    rttPasses.erase(handle);
}

void VKRenderer::serializePipelineCache() {
    auto dat = device->getPipelineCacheData(*pipelineCache);
    auto physDevProps = physicalDevice.getProperties();

    PipelineCacheDataHeader pipelineCacheHeader{};
    pipelineCacheHeader.dataSize = dat.size();
    pipelineCacheHeader.vendorID = physDevProps.vendorID;
    pipelineCacheHeader.deviceID = physDevProps.deviceID;
    pipelineCacheHeader.driverVersion = physDevProps.driverVersion;
    memcpy(pipelineCacheHeader.uuid, physDevProps.pipelineCacheUUID.data(), VK_UUID_SIZE);

    std::string pipelineCachePath = getPipelineCachePath(physDevProps);
    FILE* f = fopen(pipelineCachePath.c_str(), "wb");
    fwrite(&pipelineCacheHeader, sizeof(pipelineCacheHeader), 1, f);

    fwrite(dat.data(), dat.size(), 1, f);
    fclose(f);

    logMsg(WELogCategoryRender, "Saved pipeline cache to %s", pipelineCachePath.c_str());
}

VKRenderer::~VKRenderer() {
    if (device) {
        device->waitIdle();
        serializePipelineCache();

#ifndef NDEBUG
        char* statsString;
        vmaBuildStatsString(allocator, &statsString, true);
        std::cout << statsString << "\n";
        vmaFreeStatsString(allocator, statsString);
#endif

        for (auto& semaphore : cmdBufferSemaphores) {
            device->destroySemaphore(semaphore);
        }

        std::vector<RTTPassHandle> toDelete;
        for (auto& p : rttPasses) {
            toDelete.push_back(p.first);
        }

        for (auto& h : toDelete) {
            destroyRTTPass(h);
        }

        rttPasses.clear();
        delete irp;

        texSlots.reset();
        matSlots.reset();
        cubemapSlots.reset();

        brdfLut.destroy();

        rtResources.clear();
        loadedMeshes.clear();

#ifndef NDEBUG
        vmaBuildStatsString(allocator, &statsString, true);
        std::cout << statsString << "\n";
        vmaFreeStatsString(allocator, statsString);
#endif
        vmaDestroyAllocator(allocator);

        dbgCallback.reset();

        swapchain.reset();

        instance->destroySurfaceKHR(surface);
        logMsg(WELogCategoryRender, "Renderer destroyed.");
    }
}
