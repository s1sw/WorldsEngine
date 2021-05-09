#define _CRT_SECURE_NO_WARNINGS
#define VMA_IMPLEMENTATION
#include "../Libs/volk.h"
#include "vku/vku.hpp"
#include <vulkan/vulkan.hpp>
#include "PCH.hpp"
#include "../Core/Engine.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "../ImGui/imgui_impl_vulkan.h"
#include "../IO/physfs.hpp"
#include "../Core/Transform.hpp"
#include "tracy/Tracy.hpp"
#include "tracy/TracyC.h"
#include "tracy/TracyVulkan.hpp"
#include "RenderPasses.hpp"
#include "../Input/Input.hpp"
#include "../VR/OpenVRInterface.hpp"
#include "../Core/Fatal.hpp"
#include <unordered_set>
#include "../Core/Log.hpp"
#include "Loaders/ObjModelLoader.hpp"
#include "Render.hpp"
#include "Loaders/SourceModelLoader.hpp"
#include "Loaders/WMDLLoader.hpp"
#include "Loaders/RobloxMeshLoader.hpp"
#include "ShaderCache.hpp"
#ifdef RDOC
#include "renderdoc_app.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "../Util/EnumUtil.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace worlds;

const bool vrValidationLayers = false;

uint32_t findPresentQueue(vk::PhysicalDevice pd, vk::SurfaceKHR surface) {
    auto qprops = pd.getQueueFamilyProperties();

    for (uint32_t qi = 0; qi != qprops.size(); ++qi) {
        auto& qprop = qprops[qi];

        if (pd.getSurfaceSupportKHR(qi, surface) && enumHasFlag(qprop.queueFlags, vk::QueueFlagBits::eGraphics)) {
            return qi;
        }
    }

    return ~0u;
}

RenderTexture* VKRenderer::createRTResource(RTResourceCreateInfo resourceCreateInfo, const char* debugName) {
    return new RenderTexture{ getVKCtx(), resourceCreateInfo, debugName };
}

void VKRenderer::createSwapchain(vk::SwapchainKHR oldSwapchain) {
    bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN;
    vk::PresentModeKHR presentMode = (useVsync && !enableVR) ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eImmediate;
    QueueFamilyIndices qfi{ graphicsQueueFamilyIdx, presentQueueFamilyIdx };
    swapchain = std::make_unique<Swapchain>(physicalDevice, *device, surface, qfi, fullscreen, oldSwapchain, presentMode);
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
        auto vrInstExts = initInfo.vrInterface->getVulkanInstanceExtensions();

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
        logMsg(WELogCategoryRender, "Activating validation layers");
        instanceMaker.layer("VK_LAYER_KHRONOS_validation");
        instanceMaker.extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
#endif
    instanceMaker.extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    auto appName = initInfo.applicationName ? "Worlds Engine" : initInfo.applicationName;
    instanceMaker
        .applicationName(appName)
        .engineName("Worlds")
        .applicationVersion(1)
        .engineVersion(1);

    instance = instanceMaker.createUnique();
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
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
    }

    if (!supportedFeatures.wideLines) {
        logWarn(worlds::WELogCategoryRender, "Missing wideLines");
    }

    vk::PhysicalDeviceFeatures2 supportedFeatures2;
    vk::PhysicalDeviceVulkan11Features supportedVk11Features;
    vk::PhysicalDeviceVulkan12Features supportedVk12Features;

    supportedFeatures2.setPNext(&supportedVk11Features);
    supportedVk11Features.setPNext(&supportedVk12Features);
    physDev.getFeatures2(&supportedFeatures2);

    if (!supportedVk11Features.multiview) {
        logErr(WELogCategoryRender, "Missing multiview support");
        return false;
    }

    if (!supportedVk12Features.descriptorIndexing) {
        logErr(WELogCategoryRender, "Missing descriptor indexing");
        return false;
    }

    if (!supportedVk12Features.descriptorBindingPartiallyBound) {
        logErr(WELogCategoryRender, "Missing partially bound descriptors");
        return false;
    }

    return true;
}

bool isDeviceBetter(vk::PhysicalDevice a, vk::PhysicalDevice b) {
    auto aProps = a.getProperties();
    auto bProps = b.getProperties();

    if (bProps.deviceType == vk::PhysicalDeviceType::eDiscreteGpu &&
            aProps.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
        return true;
    } else if (aProps.deviceType == vk::PhysicalDeviceType::eDiscreteGpu &&
            bProps.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
        return false;
    }

    return aProps.deviceID < bProps.deviceID;
}

vk::PhysicalDevice pickPhysicalDevice(std::vector<vk::PhysicalDevice>& physicalDevices) {
    std::sort(physicalDevices.begin(), physicalDevices.end(), isDeviceBetter);

    return physicalDevices[0];
}

VKRenderer::VKRenderer(const RendererInitInfo& initInfo, bool* success)
    : finalPrePresent(nullptr)
    , finalPrePresentR(nullptr)
    , shadowmapImage(nullptr)
    , imguiImage(nullptr)
    , window(initInfo.window)
    , shadowmapRes(2048)
    , enableVR(initInfo.enableVR)
    , pickingPRP(nullptr)
    , vrPRP(nullptr)
    , irp(nullptr)
    , vrPredictAmount(0.033f)
    , clearMaterialIndices(false)
    , useVsync(true)
    , enablePicking(initInfo.enablePicking)
    , nextHandle(0u)
    , frameIdx(0)
    , lastFrameIdx(0) {
    maxFramesInFlight = 2;
    msaaSamples = vk::SampleCountFlagBits::e2;
    numMSAASamples = 2;

#ifdef RDOC
    if(HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdocApi);
        assert(ret == 1);
    } else {
        rdocApi = nullptr;
    }
#endif

    if (volkInitialize() != VK_SUCCESS) {
        fatalErr("Couldn't find Vulkan.");
    }

    vk::DynamicLoader dl;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        dl.getProcAddress<PFN_vkGetInstanceProcAddr>( "vkGetInstanceProcAddr" );

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    createInstance(initInfo);
    volkLoadInstance(*instance);

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
    vk::QueueFlags search = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;

    // Look for a queue family with both graphics and
    // compute first.
    for (uint32_t qi = 0; qi != qprops.size(); ++qi) {
        auto& qprop = qprops[qi];
        if ((qprop.queueFlags & search) == search) {
            graphicsQueueFamilyIdx = qi;
            break;
        }
    }

    // Search for async compute queue family
    asyncComputeQueueFamilyIdx = badQueue;
    for (size_t i = 0; i < qprops.size(); i++) {
        auto& qprop = qprops[i];
        if ((qprop.queueFlags & (vk::QueueFlagBits::eCompute)) == vk::QueueFlagBits::eCompute && i != graphicsQueueFamilyIdx) {
            asyncComputeQueueFamilyIdx = i;
            break;
        }
    }

    if (asyncComputeQueueFamilyIdx == badQueue)
        logWarn(worlds::WELogCategoryRender, "Couldn't find async compute queue");

    if (graphicsQueueFamilyIdx == badQueue) {
        *success = false;
        return;
    }

    vku::DeviceMaker dm{};
    dm.defaultLayers();
    dm.queue(graphicsQueueFamilyIdx);

    for (auto& ext : initInfo.additionalDeviceExtensions) {
        dm.extension(ext.c_str());
    }

    std::vector<std::string> vrDevExts;
    if (initInfo.enableVR && initInfo.activeVrApi == VrApi::OpenVR) {
        vrDevExts = initInfo.vrInterface->getVulkanDeviceExtensions(physicalDevice);
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

    vk::PhysicalDeviceVulkan11Features vk11Features;
    vk11Features.multiview = true;
    dm.setPNext(&vk11Features);

    vk::PhysicalDeviceVulkan12Features vk12Features;
    vk12Features.timelineSemaphore = true;
    vk12Features.descriptorBindingPartiallyBound = true;
    vk12Features.runtimeDescriptorArray = true;
    vk12Features.imagelessFramebuffer = true;
    vk11Features.setPNext(&vk12Features);

    try {
        device = dm.createUnique(physicalDevice);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);
    } catch (vk::FeatureNotPresentError& fpe) {
        fatalErr("Missing device features");
    }

    ShaderCache::setDevice(*device);

    VmaAllocatorCreateInfo allocatorCreateInfo;
    memset(&allocatorCreateInfo, 0, sizeof(allocatorCreateInfo));
    allocatorCreateInfo.device = *device;
    allocatorCreateInfo.frameInUseCount = 0;
    allocatorCreateInfo.instance = *instance;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorCreateInfo.flags = 0;
    vmaCreateAllocator(&allocatorCreateInfo, &allocator);

    vk::PipelineCacheCreateInfo pipelineCacheInfo;
    PipelineCacheSerializer::loadPipelineCache(physicalDevice.getProperties(), pipelineCacheInfo);
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
        logMsg(worlds::WELogCategoryRender, "Queue family with properties %s (supports present: %i)",
            vk::to_string(qprop.queueFlags).c_str(), physicalDevice.getSurfaceSupportKHR(qfi, surface));
        qfi++;
    }

    // Command pool
    vk::CommandPoolCreateInfo cpci;
    cpci.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    cpci.queueFamilyIndex = graphicsQueueFamilyIdx;
    commandPool = device->createCommandPoolUnique(cpci);

    createSwapchain(nullptr);

    if (initInfo.activeVrApi == VrApi::OpenVR) {
        OpenVRInterface* vrInterface = static_cast<OpenVRInterface*>(initInfo.vrInterface);
        vrInterface->getRenderResolution(&renderWidth, &renderHeight);
    }

    handles = VulkanHandles{
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

    auto vkCtx = std::make_shared<VulkanHandles>(handles);

    cubemapConvoluter = std::make_shared<CubemapConvoluter>(vkCtx);

    texSlots = std::make_unique<TextureSlots>(vkCtx);
    matSlots = std::make_unique<MaterialSlots>(vkCtx, *texSlots);
    cubemapSlots = std::make_unique<CubemapSlots>(vkCtx, cubemapConvoluter);

    vk::ImageCreateInfo brdfLutIci{
        vk::ImageCreateFlags{},
        vk::ImageType::e2D,
        vk::Format::eR16G16Sfloat,
        vk::Extent3D { 256, 256, 1 }, 1, 1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::SharingMode::eExclusive, graphicsQueueFamilyIdx
    };

    brdfLut = vku::GenericImage{ *device, allocator, brdfLutIci, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, false, "BRDF LUT" };

    vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [&](auto cb) {
        brdfLut.setLayout(cb, vk::ImageLayout::eColorAttachmentOptimal);
        });

    BRDFLUTRenderer brdfLutRenderer{ *vkCtx };
    brdfLutRenderer.render(*vkCtx, brdfLut);

    vk::ImageCreateInfo shadowmapIci;
    shadowmapIci.imageType = vk::ImageType::e2D;
    shadowmapIci.extent = vk::Extent3D{ shadowmapRes, shadowmapRes, 1 };
    shadowmapIci.arrayLayers = 3;
    shadowmapIci.mipLevels = 1;
    shadowmapIci.format = vk::Format::eD32Sfloat;
    shadowmapIci.initialLayout = vk::ImageLayout::eUndefined;
    shadowmapIci.samples = vk::SampleCountFlagBits::e1;
    shadowmapIci.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;

    RTResourceCreateInfo shadowmapCreateInfo{ shadowmapIci, vk::ImageViewType::e2DArray, vk::ImageAspectFlagBits::eDepth };
    shadowmapImage = createRTResource(shadowmapCreateInfo, "Shadowmap Image");
    shadowmapIci.arrayLayers = 1;

    shadowmapIci.extent = vk::Extent3D { 512, 512, 1 };
    for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
        RTResourceCreateInfo shadowCreateInfo{ shadowmapIci, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eDepth };
        shadowImages[i] = createRTResource(shadowCreateInfo, ("Shadow Image " + std::to_string(i)).c_str());
    }

    vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [&](auto cb) {
        shadowmapImage->image.setLayout(cb, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth);
    });

    createSCDependents();

    vk::CommandBufferAllocateInfo cbai;
    cbai.commandPool = *commandPool;
    cbai.commandBufferCount = maxFramesInFlight;
    cbai.level = vk::CommandBufferLevel::ePrimary;
    cmdBufs = device->allocateCommandBuffersUnique(cbai);

    for (size_t i = 0; i < cmdBufs.size(); i++) {
        vk::FenceCreateInfo fci;
        fci.flags = vk::FenceCreateFlagBits::eSignaled;
        cmdBufFences.push_back(device->createFence(fci));

        vk::SemaphoreCreateInfo sci;
        cmdBufferSemaphores.push_back(device->createSemaphore(sci));
        imgAvailable.push_back(device->createSemaphore(sci));
    }
    imgFences.resize(cmdBufs.size());

    timestampPeriod = physicalDevice.getProperties().limits.timestampPeriod;

    vk::QueryPoolCreateInfo qpci{};
    qpci.queryType = vk::QueryType::eTimestamp;
    qpci.queryCount = 2 * maxFramesInFlight;
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

    // Load cubemap for the sky
    cubemapSlots->loadOrGet(g_assetDB.addOrGetExisting("envmap_miramar/miramar.json"));

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
        auto file = PHYSFS_openWrite("memory.json");
        PHYSFS_writeBytes(file, statsString, strlen(statsString));
        PHYSFS_close(file);
        vmaFreeStatsString(allocator, statsString);
        }, "r_printAllocInfo", "", nullptr);

    SlotArrays slotArrays { *texSlots, *cubemapSlots, *matSlots };

    PassSetupCtx psc{
        &materialUB,
        getVKCtx(),
        slotArrays,
        (int)swapchain->images.size(),
        enableVR,
        &brdfLut,
        renderWidth,
        renderHeight
    };

    shadowCascadePass = new ShadowCascadePass(shadowmapImage);
    shadowCascadePass->setup(psc);

    materialUB = vku::UniformBuffer(*device, allocator, sizeof(MaterialsUB), VMA_MEMORY_USAGE_GPU_ONLY, "Materials");

    MaterialsUB materials;
    materialUB.upload(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), &materials, sizeof(materials));

}

// Quite a lot of resources are dependent on either the number of images
// there are in the swap chain or the swapchain itself, so they need to be
// recreated whenever the swap chain changes.
void VKRenderer::createSCDependents() {
    delete imguiImage;
    delete finalPrePresent;
    delete finalPrePresentR;
    SlotArrays slotArrays { *texSlots, *cubemapSlots, *matSlots };

    PassSetupCtx psc{
        &materialUB,
        getVKCtx(),
        slotArrays,
        (int)swapchain->images.size(),
        enableVR,
        &brdfLut,
        renderWidth,
        renderHeight
    };

    if (irp == nullptr) {
        irp = new ImGuiRenderPass(*swapchain);
        irp->setup(psc);
    }

    createFramebuffers();

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
        finalPrePresent->image.setLayout(cmdBuf, vk::ImageLayout::eTransferSrcOptimal);
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

    imgFences.clear();
    imgFences.resize(swapchain->images.size());

    for (auto& s : imgAvailable) {
        device->destroySemaphore(s);
    }
    imgAvailable.clear();

    for (size_t i = 0; i < cmdBufs.size(); i++) {
        vk::SemaphoreCreateInfo sci;
        imgAvailable.push_back(device->createSemaphore(sci));
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

    swapchain.reset();
    framebuffers.clear();

    createSwapchain(nullptr);
    createSCDependents();

    swapchainRecreated = true;
}

void VKRenderer::presentNothing(uint32_t imageIndex) {
    vk::Semaphore imgSemaphore = imgAvailable[frameIdx];
    vk::Semaphore cmdBufSemaphore = cmdBufferSemaphores[frameIdx];

    vk::PresentInfoKHR presentInfo;
    vk::SwapchainKHR cSwapchain = *swapchain->getSwapchain();
    presentInfo.pSwapchains = &cSwapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imageIndex;

    presentInfo.pWaitSemaphores = &cmdBufSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    vk::CommandBufferBeginInfo cbbi;
    auto& cmdBuf = cmdBufs[frameIdx];
    cmdBuf->begin(cbbi);
    cmdBuf->end();

    vk::SubmitInfo submitInfo;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imgSemaphore;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &cmdBufferSemaphores[frameIdx];
    vk::CommandBuffer cCmdBuf = *cmdBuf;
    submitInfo.pCommandBuffers = &cCmdBuf;
    submitInfo.commandBufferCount = 1;
    device->getQueue(presentQueueFamilyIdx, 0).submit(submitInfo, nullptr);

    auto presentResult = device->getQueue(presentQueueFamilyIdx, 0).presentKHR(presentInfo);
    if (presentResult != vk::Result::eSuccess && presentResult != vk::Result::eSuboptimalKHR)
        fatalErr("Present failed!");
}

void imageBarrier(vk::CommandBuffer& cb, vk::Image image, vk::ImageLayout layout,
        vk::AccessFlags srcMask, vk::AccessFlags dstMask,
        vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask,
        vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor, uint32_t numLayers = 1) {
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

void VKRenderer::submitToOpenVR() {
    // Submit to SteamVR
    vr::VRTextureBounds_t bounds {
        .uMin = 0.0f,
        .vMin = 0.0f,
        .uMax = 1.0f,
        .vMax = 1.0f
    };

    VkImage vkImg = finalPrePresent->image.image();

    vr::VRVulkanTextureData_t vulkanData {
        .m_nImage = (uint64_t)vkImg,
        .m_pDevice = (VkDevice_T*)*device,
        .m_pPhysicalDevice = (VkPhysicalDevice_T*)physicalDevice,
        .m_pInstance = (VkInstance_T*)*instance,
        .m_pQueue = (VkQueue_T*)device->getQueue(graphicsQueueFamilyIdx, 0),
        .m_nQueueFamilyIndex = graphicsQueueFamilyIdx,
        .m_nWidth = renderWidth,
        .m_nHeight = renderHeight,
        .m_nFormat = VK_FORMAT_R8G8B8A8_UNORM,
        .m_nSampleCount = 1
    };

    // Image submission with validation layers turned on causes a crash
    // If we really want the validation layers, don't submit anything
    if (!vrValidationLayers) {
        vr::Texture_t texture = { &vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Auto };
        vr::VRCompositor()->Submit(vr::Eye_Left, &texture, &bounds);

        vulkanData.m_nImage = (uint64_t)(VkImage)finalPrePresentR->image.image();
        vr::VRCompositor()->Submit(vr::Eye_Right, &texture, &bounds);
    }
}

void VKRenderer::uploadSceneAssets(entt::registry& reg) {
    ZoneScoped;
    bool reuploadMats = false;

    // Upload any necessary materials + meshes
    reg.view<WorldObject>().each([&](auto, WorldObject& wo) {
        for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
            if (!wo.presentMaterials[i]) continue;

            if (wo.materialIdx[i] == ~0u) {
                reuploadMats = true;
                wo.materialIdx[i] = matSlots->loadOrGet(wo.materials[i]);
            }
        }

        if (loadedMeshes.find(wo.mesh) == loadedMeshes.end()) {
            preloadMesh(wo.mesh);
        }
    });

    reg.view<ProceduralObject>().each([&](auto, ProceduralObject& po) {
        if (po.materialIdx == ~0u) {
            reuploadMats = true;
            po.materialIdx = matSlots->loadOrGet(po.material);
        }
    });

    reg.view<WorldCubemap>().each([&](auto, WorldCubemap& wc) {
        if (!cubemapSlots->isLoaded(wc.cubemapId)) {
            cubemapSlots->loadOrGet(wc.cubemapId);
            reuploadMats = true;
        }
    });

    if (reuploadMats)
        reuploadMaterials();
}

glm::mat4 VKRenderer::getCascadeMatrix(Camera cam, glm::vec3 lightDir, glm::mat4 frustumMatrix, float& texelsPerUnit) {
    glm::mat4 view;

    if (!enableVR) {
        view = cam.getViewMatrix();
    } else {
        view = glm::inverse(vrInterface->getHeadTransform(vrPredictAmount)) * cam.getViewMatrix();
    }

    glm::mat4 vpInv = glm::inverse(frustumMatrix * view);

    glm::vec3 frustumCorners[8] = {
        glm::vec3(-1.0f,  1.0f, -1.0f),
        glm::vec3( 1.0f,  1.0f, -1.0f),
        glm::vec3( 1.0f, -1.0f, -1.0f),
        glm::vec3(-1.0f, -1.0f, -1.0f),
        glm::vec3(-1.0f,  1.0f,  1.0f),
        glm::vec3( 1.0f,  1.0f,  1.0f),
        glm::vec3( 1.0f, -1.0f,  1.0f),
        glm::vec3(-1.0f, -1.0f,  1.0f),
    };

    for (int i = 0; i < 8; i++) {
        glm::vec4 transformed = vpInv * glm::vec4{ frustumCorners[i], 1.0f };
        transformed /= transformed.w;
        frustumCorners[i] = transformed;
    }

    glm::vec3 center{0.0f};

    for (int i = 0; i < 8; i++) {
        center += frustumCorners[i];
    }

    center /= 8.0f;

    float diameter = 0.0f;
    for (int i = 0; i < 8; i++) {
        float dist = glm::length(frustumCorners[i] - center);
        diameter = glm::max(diameter, dist);
    }
    float radius = diameter * 0.5f;

    texelsPerUnit = (float)shadowmapRes / diameter;

    glm::mat4 scaleMatrix = glm::scale(glm::mat4{1.0f}, glm::vec3{texelsPerUnit});

    glm::mat4 lookAt = glm::lookAt(glm::vec3{0.0f}, lightDir, glm::vec3{ 0.0f, 1.0f, 0.0f });
    lookAt *= scaleMatrix;

    glm::mat4 lookAtInv = glm::inverse(lookAt);

    center = lookAt * glm::vec4{ center, 1.0f };
    center = glm::floor(center);
    center = lookAtInv * glm::vec4 { center, 1.0f };

    glm::vec3 eye = center + (lightDir * diameter);

    glm::mat4 viewMat = glm::lookAt(eye, center, glm::vec3{ 0.0f, 1.0f, 0.0f });
    glm::mat4 projMat = glm::orthoZO(-radius, radius, -radius, radius, -radius * 12.0f, radius * 12.0f);

    return projMat * viewMat;
}

worlds::ConVar doGTAO{ "r_doGTAO", "0" };

void VKRenderer::calculateCascadeMatrices(entt::registry& world, RenderCtx& rCtx) {
    world.view<WorldLight, Transform>().each([&](auto, WorldLight& l, Transform& transform) {
        glm::vec3 lightForward = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        if (l.type == LightType::Directional) {
            glm::mat4 frustumMatrices[3];
            float aspect = (float)rCtx.width / (float)rCtx.height;
            // frustum 0: near -> 20m
            // frustum 1: 20m  -> 125m
            // frustum 2: 125m -> 250m
            float splits[4] = { rCtx.cam->near, 15.0f, 60.0f, 140.0f };
            if (!rCtx.enableVR) {
                for (int i = 1; i < 4; i++) {
                    frustumMatrices[i - 1] = glm::perspective(
                        rCtx.cam->verticalFOV, aspect,
                        splits[i - 1], splits[i]
                    );
                }
            } else {
                for (int i = 1; i < 4; i++) {
                    frustumMatrices[i - 1] = vrInterface->getEyeProjectionMatrix(
                        Eye::LeftEye,
                        splits[i - 1], splits[i]
                    );
                }
            }

            for (int i = 0; i < 3; i++) {
                rCtx.cascadeShadowMatrices[i] =
                    getCascadeMatrix(
                        *rCtx.cam, lightForward,
                        frustumMatrices[i], rCtx.cascadeTexelsPerUnit[i]
                    );
            }
        }
    });
}

void VKRenderer::writePassCmds(RTTPassHandle pass, vk::CommandBuffer cmdBuf, entt::registry& world) {
    auto& rtt = rttPasses.at(pass);
    SlotArrays slotArrays { *texSlots, *cubemapSlots, *matSlots };
    PassSetupCtx psc {
        &materialUB, getVKCtx(), slotArrays,
        (int)swapchain->images.size(), enableVR, &brdfLut, rtt.width, rtt.height };

    RenderCtx rCtx{ cmdBuf, world, 0, rtt.cam, slotArrays, rtt.width, rtt.height, loadedMeshes };
    rCtx.enableShadows = rtt.enableShadows;
    rCtx.enableVR = rtt.isVr;
    rCtx.viewPos = rtt.cam->position;
    rCtx.dbgStats = &dbgStats;
    rCtx.shadowImages = shadowImages;

    if (enableVR) {
        rCtx.vrProjMats[0] = vrInterface->getEyeProjectionMatrix(Eye::LeftEye, rtt.cam->near);
        rCtx.vrProjMats[1] = vrInterface->getEyeProjectionMatrix(Eye::RightEye, rtt.cam->near);

        glm::mat4 hmdMat = vrInterface->getHeadTransform(vrPredictAmount);

        for (int i = 0; i < 2; i++) {
            rCtx.vrViewMats[i] = glm::inverse(hmdMat) * rtt.cam->getViewMatrix();
        }
    }

    if (rtt.enableShadows) {
        calculateCascadeMatrices(world, rCtx);
        shadowCascadePass->prePass(psc, rCtx);
        shadowCascadePass->execute(rCtx);
    }

    rtt.prp->prePass(psc, rCtx);
    rtt.prp->execute(rCtx);

    if (doGTAO.getInt())
        rtt.gtrp->execute(rCtx);
    else
        rtt.gtaoOut->image.setLayout(cmdBuf, vk::ImageLayout::eShaderReadOnlyOptimal);

    rtt.hdrTarget->image.barrier(cmdBuf,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead);

    rtt.trp->execute(rCtx);
}

void VKRenderer::writeCmdBuf(vk::UniqueCommandBuffer& cmdBuf, uint32_t imageIndex, Camera& cam, entt::registry& reg) {
    ZoneScoped;

#ifdef TRACY_ENABLE
    rCtx.tracyContexts = &tracyContexts;
#endif

    vk::CommandBufferBeginInfo cbbi;
    cbbi.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    cmdBuf->begin(cbbi);
    texSlots->frameStarted = true;
    cmdBuf->resetQueryPool(*queryPool, 0 + (2 * frameIdx), 2);
    cmdBuf->writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0 + (2 * frameIdx));

    texSlots->setUploadCommandBuffer(*cmdBuf, frameIdx);

    if (clearMaterialIndices) {
        reg.view<WorldObject>().each([](entt::entity, WorldObject& wo) {
            memset(wo.materialIdx, ~0u, sizeof(wo.materialIdx));
        });
        clearMaterialIndices = false;
    }

    uploadSceneAssets(reg);

    finalPrePresent->image.setLayout(*cmdBuf,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eColorAttachmentWrite);

    int numActivePasses = 0;
    for (auto& p : rttPasses) {
        if (!p.second.active) continue;
        numActivePasses++;

        if (!p.second.outputToScreen) {
            p.second.sdrFinalTarget->image.setLayout(*cmdBuf,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eColorAttachmentWrite);
        }

        bool nullCam = p.second.cam == nullptr;

        if (nullCam)
            p.second.cam = &cam;

        writePassCmds(p.first, *cmdBuf, reg);

        if (nullCam)
            p.second.cam = nullptr;

        if (!p.second.outputToScreen) {
            p.second.sdrFinalTarget->image.setLayout(*cmdBuf,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
                vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead);
        }
    }
    dbgStats.numRTTPasses = numActivePasses;

    vku::transitionLayout(*cmdBuf, swapchain->images[imageIndex],
        vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer,
        vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite);

    finalPrePresent->image.setLayout(*cmdBuf,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead);

    if (enableVR) {
        finalPrePresentR->image.setLayout(*cmdBuf,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead);
    }

    cmdBuf->clearColorImage(
        swapchain->images[imageIndex],
        vk::ImageLayout::eTransferDstOptimal,
        vk::ClearColorValue{ std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } },
        vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    );

    if (!enableVR) {
        vk::ImageBlit imageBlit;
        imageBlit.srcOffsets[1] = imageBlit.dstOffsets[1] = vk::Offset3D{ (int)width, (int)height, 1 };
        imageBlit.dstSubresource = imageBlit.srcSubresource = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
        cmdBuf->blitImage(
            finalPrePresent->image.image(), vk::ImageLayout::eTransferSrcOptimal,
            swapchain->images[imageIndex], vk::ImageLayout::eTransferDstOptimal,
            imageBlit, vk::Filter::eNearest);
    } else {
        // Calculate the best crop for the current window size against the VR render target
        //float scaleFac = glm::min((float)windowSize.x / renderWidth, (float)windowSize.y / renderHeight);
        float aspect = (float)windowSize.y / (float)windowSize.x;
        float croppedHeight = aspect * renderWidth;

        glm::vec2 srcCorner0(0.0f, renderHeight / 2 - croppedHeight / 2.0f);
        glm::vec2 srcCorner1(renderWidth, renderHeight / 2 + croppedHeight / 2.0f);


        vk::ImageBlit imageBlit;
        imageBlit.srcOffsets[0] = vk::Offset3D{ (int)srcCorner0.x, (int)srcCorner0.y, 0 };
        imageBlit.srcOffsets[1] = vk::Offset3D{ (int)srcCorner1.x, (int)srcCorner1.y, 1 };
        imageBlit.dstOffsets[1] = vk::Offset3D{ (int)windowSize.x, (int)windowSize.y, 1 };
        imageBlit.dstSubresource = imageBlit.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };

        cmdBuf->blitImage(
            finalPrePresent->image.image(), vk::ImageLayout::eTransferSrcOptimal,
            swapchain->images[imageIndex], vk::ImageLayout::eTransferDstOptimal,
            imageBlit, vk::Filter::eLinear);
    }

    vku::transitionLayout(*cmdBuf, swapchain->images[imageIndex],
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead);

    irp->execute(*cmdBuf, width, height, *framebuffers[imageIndex]);

    ::imageBarrier(*cmdBuf, swapchain->images[imageIndex], vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead, vk::AccessFlagBits::eMemoryRead,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe);

    cmdBuf->writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *queryPool, 1 + (frameIdx * 2));
#ifdef TRACY_ENABLE
    TracyVkCollect(tracyContexts[imageIndex], *cmdBuf);
#endif
    texSlots->frameStarted = false;
    cmdBuf->end();

    if (enableVR && vrApi == VrApi::OpenVR) {
        glm::mat4 headViewMatrix = vrInterface->getHeadTransform(vrPredictAmount);

        glm::mat4 viewMats[2] = {
            vrInterface->getEyeViewMatrix(Eye::LeftEye),
            vrInterface->getEyeViewMatrix(Eye::RightEye)
        };

        glm::vec3 viewPos[2];

        for (int i = 0; i < 2; i++) {
            viewMats[i] = glm::inverse(headViewMatrix * viewMats[i]) * cam.getViewMatrix();
            viewPos[i] = glm::inverse(viewMats[i])[3];
        }

        if (vrPRP)
            vrPRP->lateUpdateVP(viewMats, viewPos, *device);

        vr::VRCompositor()->SubmitExplicitTimingData();
    }
}

void VKRenderer::reuploadMaterials() {
    materialUB.upload(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), matSlots->getSlots(), sizeof(PackedMaterial) * 256);

    for (auto& pair : this->rttPasses) {
        pair.second.prp->reuploadDescriptors();
    }
}

ConVar showSlotDebug { "r_debugSlots", "0", "Shows a window for debugging resource slots." };

void VKRenderer::frame(Camera& cam, entt::registry& reg) {
    ZoneScoped;

    if (showSlotDebug.getInt()) {
        if (ImGui::Begin("Render Slot Debug")) {
            if (ImGui::CollapsingHeader("Cubemap Slots")) {
                for (uint32_t i = 0; i < NUM_CUBEMAP_SLOTS; i++) {
                    if (cubemapSlots->isSlotPresent(i)) {
                        ImGui::Text("Slot %u: %s", i, g_assetDB.getAssetPath(cubemapSlots->getKeyForSlot(i)).c_str());
                    }
                }
            }

            if (ImGui::CollapsingHeader("Texture Slots")) {
                for (uint32_t i = 0; i< NUM_TEX_SLOTS; i++) {
                    if (texSlots->isSlotPresent(i)) {
                        ImGui::Text("Slot %u: %s", i, g_assetDB.getAssetPath(texSlots->getKeyForSlot(i)).c_str());
                    }
                }
            }

            if (ImGui::CollapsingHeader("Material Slots")) {
                for (uint32_t i = 0; i< NUM_MAT_SLOTS; i++) {
                    if (matSlots->isSlotPresent(i)) {
                        ImGui::Text("Slot %u: %s", i, g_assetDB.getAssetPath(matSlots->getKeyForSlot(i)).c_str());
                    }
                }
            }
        }
        ImGui::End();
    }

    bool recreate = false;

    dbgStats.numCulledObjs = 0;
    dbgStats.numDrawCalls = 0;
    dbgStats.numPipelineSwitches = 0;
    dbgStats.numTriangles = 0;
    destroyTempTexBuffers(frameIdx);

    uint32_t imageIndex;

    vk::Result nextImageRes = swapchain->acquireImage(*device, imgAvailable[frameIdx], &imageIndex);

    if ((nextImageRes == vk::Result::eErrorOutOfDateKHR || nextImageRes == vk::Result::eSuboptimalKHR) && width != 0 && height != 0) {
        if (nextImageRes == vk::Result::eErrorOutOfDateKHR)
            logMsg("Swapchain out of date");
        else
            logMsg("Swapchain suboptimal");
        recreateSwapchain();

        // acquire image from new swapchain
        swapchain->acquireImage(*device, imgAvailable[frameIdx], &imageIndex);
    }

    if (imgFences[imageIndex] && imgFences[imageIndex] != cmdBufFences[frameIdx]) {
        if (device->waitForFences(imgFences[imageIndex], true, UINT64_MAX) != vk::Result::eSuccess) {
            fatalErr("Failed to wait on image fence");
        }
    }

    imgFences[imageIndex] = cmdBufFences[frameIdx];

    if (device->waitForFences(1, &cmdBufFences[frameIdx], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess) {
        fatalErr("Failed to wait on fences");
    }

    if (device->resetFences(1, &cmdBufFences[frameIdx]) != vk::Result::eSuccess) {
        fatalErr("Failed to reset fences");
    }

    auto& cmdBuf = cmdBufs[frameIdx];
    writeCmdBuf(cmdBuf, imageIndex, cam, reg);

    vk::SubmitInfo submit;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &imgAvailable[frameIdx];

    vk::PipelineStageFlags waitStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    submit.pWaitDstStageMask = &waitStages;

    submit.commandBufferCount = 1;
    vk::CommandBuffer cCmdBuf = *cmdBuf;
    submit.pCommandBuffers = &cCmdBuf;
    submit.pSignalSemaphores = &cmdBufferSemaphores[frameIdx];
    submit.signalSemaphoreCount = 1;

    auto queue = device->getQueue(graphicsQueueFamilyIdx, 0);
    auto submitResult = queue.submit(1, &submit, cmdBufFences[frameIdx]);

    if (submitResult != vk::Result::eSuccess) {
        std::string errStr = vk::to_string(submitResult);
        fatalErr(("Failed to submit queue (error: " + errStr + ")").c_str());
    }

    TracyMessageL("Queue submitted");

    if (enableVR) {
        submitToOpenVR();
    }

    vk::PresentInfoKHR presentInfo;
    vk::SwapchainKHR cSwapchain = *swapchain->getSwapchain();
    presentInfo.pSwapchains = &cSwapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imageIndex;

    presentInfo.pWaitSemaphores = &cmdBufferSemaphores[frameIdx];
    presentInfo.waitSemaphoreCount = 1;

    try {
        vk::Result presentResult = queue.presentKHR(presentInfo);

        if (presentResult == vk::Result::eSuboptimalKHR) {
            logMsg("swapchain after present suboptimal");
            //recreateSwapchain();
        } else if (presentResult != vk::Result::eSuccess) {
            fatalErr("Failed to present");
        }
    } catch (const vk::OutOfDateKHRError&) {
        recreateSwapchain();
    }

    TracyMessageL("Presented");

    if (vrApi == VrApi::OpenVR)
        vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);

    std::array<std::uint64_t, 2> timeStamps = { {0} };

    auto queryRes = device->getQueryPoolResults(
        *queryPool, 2 * lastFrameIdx, (uint32_t)timeStamps.size(),
        timeStamps.size() * sizeof(uint64_t), timeStamps.data(),
        sizeof(uint64_t), vk::QueryResultFlagBits::e64
    );

    if (queryRes == vk::Result::eSuccess)
        lastRenderTimeTicks = timeStamps[1] - timeStamps[0];

    if (recreate)
        recreateSwapchain();

    lastFrameIdx = frameIdx;

    frameIdx++;
    frameIdx %= maxFramesInFlight;
    FrameMark;
}

void VKRenderer::preloadMesh(AssetID id) {
    ZoneScoped;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    auto ext = g_assetDB.getAssetExtension(id);
    auto path = g_assetDB.getAssetPath(id);
    LoadedMeshData lmd;

    if (!PHYSFS_exists(path.c_str())) {
        logErr(WELogCategoryRender, "Mesh %s was missing!", path.c_str());
        PhysFS::ifstream meshFileStream(g_assetDB.openAssetFileRead(g_assetDB.addOrGetExisting("Models/missing.obj")));
        loadObj(vertices, indices, meshFileStream, lmd);
        lmd.numSubmeshes = 1;
        lmd.submeshes[0].indexCount = indices.size();
        lmd.submeshes[0].indexOffset = 0;
    } else if (ext == ".obj") { // obj
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
    } else if (ext == ".rblx") {
        loadRobloxMesh(id, vertices, indices, lmd);
    }

    lmd.indexType = vk::IndexType::eUint32;
    lmd.indexCount = (uint32_t)indices.size();
    lmd.ib = vku::IndexBuffer{ *device, allocator, indices.size() * sizeof(uint32_t), "Mesh Index Buffer" };
    lmd.ib.upload(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), indices);
    lmd.vb = vku::VertexBuffer{ *device, allocator, vertices.size() * sizeof(Vertex), "Mesh Vertex Buffer" };
    lmd.vb.upload(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), vertices);

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
    procObj.indexType = vk::IndexType::eUint32;
    procObj.indexCount = (uint32_t)procObj.indices.size();
    procObj.ib = vku::IndexBuffer{ *device, allocator, procObj.indices.size() * sizeof(uint32_t), procObj.dbgName.c_str() };
    procObj.ib.upload(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), procObj.indices);
    procObj.vb = vku::VertexBuffer{ *device, allocator, procObj.vertices.size() * sizeof(Vertex), procObj.dbgName.c_str() };
    procObj.vb.upload(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), procObj.vertices);
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

            uint32_t normalTex = (*matSlots)[wo.materialIdx[i]].normalTexIdx;

            if (normalTex != ~0u) {
                textureReferenced[normalTex] = true;
            }

            uint32_t heightmapTex = (*matSlots)[wo.materialIdx[i]].heightmapTexIdx;

            if (heightmapTex != ~0u) {
                textureReferenced[heightmapTex] = true;
            }

            uint32_t metalMapTex = (*matSlots)[wo.materialIdx[i]].metalTexIdx;

            if (metalMapTex != ~0u) {
                textureReferenced[metalMapTex] = true;
            }

            uint32_t roughTexIdx = (*matSlots)[wo.materialIdx[i]].roughTexIdx;

            if (roughTexIdx != ~0u) {
                textureReferenced[roughTexIdx] = true;
            }

            uint32_t aoTexIdx = (*matSlots)[wo.materialIdx[i]].aoTexIdx;

            if (aoTexIdx != ~0u) {
                textureReferenced[aoTexIdx] = true;
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

void VKRenderer::reloadContent(ReloadFlags flags) {
    device->waitIdle();
    if (enumHasFlag(flags, ReloadFlags::Materials)) {
        for (uint32_t i = 0; i < NUM_MAT_SLOTS; i++) {
            if (matSlots->isSlotPresent(i))
                matSlots->unload(i);
        }
    }

    if (enumHasFlag(flags, ReloadFlags::Textures)) {
        for (uint32_t i = 0; i < NUM_TEX_SLOTS; i++) {
            if (texSlots->isSlotPresent(i))
                texSlots->unload(i);
        }
    }

    if (enumHasFlag(flags, ReloadFlags::Cubemaps)) {
        // ignore skybox
        for (uint32_t i = 1; i < NUM_CUBEMAP_SLOTS; i++) {
            if (cubemapSlots->isSlotPresent(i))
                cubemapSlots->unload(i);
        }
    }

    clearMaterialIndices = true;
    if (enumHasFlag(flags, ReloadFlags::Meshes)) {
        loadedMeshes.clear();
    }
}

const VulkanHandles& VKRenderer::getVKCtx() {
    return handles;
}

RTTPassHandle VKRenderer::createRTTPass(RTTPassCreateInfo& ci) {
    RTTPassInternal rpi;
    rpi.cam = ci.cam;

    vk::ImageCreateInfo ici;
    ici.imageType = vk::ImageType::e2D;
    ici.extent = vk::Extent3D{ ci.width, ci.height, 1 };
    ici.arrayLayers = ci.isVr ? 2 : 1;
    ici.mipLevels = 1;
    ici.format = vk::Format::eB10G11R11UfloatPack32;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    ici.samples = msaaSamples;
    ici.usage =
          vk::ImageUsageFlagBits::eColorAttachment
        | vk::ImageUsageFlagBits::eSampled
        | vk::ImageUsageFlagBits::eStorage
        | vk::ImageUsageFlagBits::eTransferSrc;

    RTResourceCreateInfo polyCreateInfo{ ici, vk::ImageViewType::e2DArray, vk::ImageAspectFlagBits::eColor };
    rpi.hdrTarget = createRTResource(polyCreateInfo, "HDR Target");

    ici.format = vk::Format::eD32Sfloat;
    ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
    RTResourceCreateInfo depthCreateInfo{ ici, vk::ImageViewType::e2DArray, vk::ImageAspectFlagBits::eDepth };
    rpi.depthTarget = createRTResource(depthCreateInfo, "Depth Stencil Image");

    {
        auto prp = new PolyRenderPass(rpi.depthTarget, rpi.hdrTarget, shadowmapImage, enablePicking);
        if (ci.useForPicking)
            pickingPRP = prp;
        if (ci.isVr)
            vrPRP = prp;
        rpi.prp = prp;
    }

    ici.samples = vk::SampleCountFlagBits::e1;
    ici.format = vk::Format::eR8Unorm;
    ici.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
    RTResourceCreateInfo gtaoTarget{ ici, vk::ImageViewType::e2DArray, vk::ImageAspectFlagBits::eColor };
    rpi.gtaoOut = createRTResource(gtaoTarget, "GTAO Target");

    ici.arrayLayers = 1;
    ici.format = vk::Format::eR8G8B8A8Unorm;
    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;

    if (!ci.outputToScreen) {
        RTResourceCreateInfo sdrTarget{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };
        rpi.sdrFinalTarget = createRTResource(sdrTarget, "SDR Target");
    }

    SlotArrays slotArrays { *texSlots, *cubemapSlots, *matSlots };
    PassSetupCtx psc{ &materialUB, getVKCtx(), slotArrays, (int)swapchain->images.size(), ci.isVr, &brdfLut,
    ci.width, ci.height };
    auto tonemapRP = new TonemapRenderPass(rpi.hdrTarget, ci.outputToScreen ? finalPrePresent : rpi.sdrFinalTarget, rpi.gtaoOut);
    rpi.trp = tonemapRP;

    vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [&](vk::CommandBuffer cmdBuf) {
        rpi.hdrTarget->image.setLayout(cmdBuf, vk::ImageLayout::eGeneral);
        if (!ci.outputToScreen)
            rpi.sdrFinalTarget->image.setLayout(cmdBuf, vk::ImageLayout::eShaderReadOnlyOptimal);
        if (ci.isVr) {
            finalPrePresentR->image.setLayout(cmdBuf, vk::ImageLayout::eTransferSrcOptimal);
        }
        });

    rpi.gtrp = new GTAORenderPass{ this, rpi.depthTarget, rpi.gtaoOut };
    rpi.trp->setup(psc);
    rpi.prp->setup(psc);
    rpi.gtrp->setup(psc);

    if (ci.isVr) {
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

    delete rpi.prp;
    delete rpi.trp;
    delete rpi.gtrp;

    delete rpi.hdrTarget;
    delete rpi.depthTarget;
    delete rpi.gtaoOut;

    if (!rpi.outputToScreen)
        delete rpi.sdrFinalTarget;

    rttPasses.erase(handle);
}

float* VKRenderer::getPassHDRData(RTTPassHandle handle) {
    auto& rtt = rttPasses.at(handle);

    if (rtt.isVr) {
        logErr("Getting pass data for VR passes is not supported");
        return nullptr;
    }

    vk::ImageCreateInfo ici;
    ici.imageType = vk::ImageType::e2D;
    ici.extent = vk::Extent3D{ rtt.width, rtt.height, 1 };
    ici.arrayLayers = 1;
    ici.mipLevels = 1;
    ici.format = vk::Format::eR32G32B32A32Sfloat;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.tiling = vk::ImageTiling::eOptimal;
    ici.usage =
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;

    vku::GenericImage targetImg{
        *device, allocator, ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor,
        false, "Transfer Destination" };

    ici.tiling = vk::ImageTiling::eOptimal;
    ici.format = vk::Format::eB10G11R11UfloatPack32;

    vku::GenericImage resolveImg{
        *device, allocator, ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor,
        false, "Resolve Target" };

    size_t imgSize = rtt.width * rtt.height * sizeof(float) * 4;
    vku::GenericBuffer outputBuffer {
        *device, allocator, vk::BufferUsageFlagBits::eTransferDst, imgSize,
        VMA_MEMORY_USAGE_GPU_TO_CPU, "Output Buffer"
    };

    vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [&](vk::CommandBuffer cmdBuf) {
        targetImg.setLayout(cmdBuf,
                vk::ImageLayout::eTransferDstOptimal,
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferWrite,
                vk::AccessFlagBits::eTransferWrite);

        resolveImg.setLayout(cmdBuf,
                vk::ImageLayout::eTransferDstOptimal,
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferWrite,
                vk::AccessFlagBits::eTransferWrite);

        auto oldHdrLayout = rtt.hdrTarget->image.layout();
        rtt.hdrTarget->image.setLayout(cmdBuf,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::PipelineStageFlagBits::eAllGraphics,
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eShaderRead,
                vk::AccessFlagBits::eTransferRead);

        vk::ImageResolve resolve;
        resolve.srcSubresource.layerCount = 1;
        resolve.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        resolve.dstSubresource.layerCount = 1;
        resolve.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        resolve.extent = vk::Extent3D { rtt.width, rtt.height, 1 };
        cmdBuf.resolveImage(
                rtt.hdrTarget->image.image(), vk::ImageLayout::eTransferSrcOptimal,
                resolveImg.image(), vk::ImageLayout::eTransferDstOptimal,
                resolve);

        resolveImg.setLayout(cmdBuf,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferWrite,
                vk::AccessFlagBits::eTransferRead);

        vk::ImageBlit blit;
        blit.srcSubresource.layerCount = 1;
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.layerCount = 1;
        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcOffsets[1] = blit.dstOffsets[1] =
            vk::Offset3D {static_cast<int32_t>(rtt.width), static_cast<int32_t>(rtt.height), 1};

        cmdBuf.blitImage(
                resolveImg.image(), vk::ImageLayout::eTransferSrcOptimal,
                targetImg.image(), vk::ImageLayout::eTransferDstOptimal,
                1,
                &blit,
                vk::Filter::eNearest);

        rtt.hdrTarget->image.setLayout(cmdBuf,
                oldHdrLayout,
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eAllGraphics,
                vk::AccessFlagBits::eTransferRead,
                vk::AccessFlagBits::eShaderRead);

        targetImg.setLayout(cmdBuf,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferWrite,
                vk::AccessFlagBits::eTransferRead);

        vk::BufferImageCopy bic;
        bic.imageSubresource.layerCount = 1;
        bic.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        bic.imageExtent = vk::Extent3D { rtt.width, rtt.height, 1 };

        cmdBuf.copyImageToBuffer(
            targetImg.image(),
            vk::ImageLayout::eTransferSrcOptimal,
            outputBuffer.buffer(), bic);
    });

    float* buffer = (float*)malloc(rtt.width * rtt.height * 4 * sizeof(float));
    char* mapped = (char*)outputBuffer.map(*device);
    memcpy(buffer, mapped, rtt.width * rtt.height * 4 * sizeof(float));
    outputBuffer.unmap(*device);

    return buffer;
}

void VKRenderer::updatePass(RTTPassHandle handle, entt::registry& world) {
    vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0),
    [&](vk::CommandBuffer cmdBuf) {
        writePassCmds(handle, cmdBuf, world);
    });
}

void VKRenderer::triggerRenderdocCapture() {
    if (!rdocApi) return;
#ifdef RDOC
    RENDERDOC_API_1_1_2* rdocApiActual = (RENDERDOC_API_1_1_2*)rdocApi;
    rdocApiActual->TriggerCapture();
#endif
}

void VKRenderer::startRdocCapture() {
    if (!rdocApi) return;
#ifdef RDOC
    RENDERDOC_API_1_1_2* rdocApiActual = (RENDERDOC_API_1_1_2*)rdocApi;
    rdocApiActual->StartFrameCapture(nullptr, nullptr);
#endif
}

void VKRenderer::endRdocCapture() {
    if (!rdocApi) return;
#ifdef RDOC
    RENDERDOC_API_1_1_2* rdocApiActual = (RENDERDOC_API_1_1_2*)rdocApi;
    rdocApiActual->EndFrameCapture(nullptr, nullptr);
#endif
}

VKRenderer::~VKRenderer() {
    if (device) {
        device->waitIdle();
        auto physDevProps = physicalDevice.getProperties();
        PipelineCacheSerializer::savePipelineCache(physDevProps, *pipelineCache, *device);

        ShaderCache::clear();

        for (auto& semaphore : cmdBufferSemaphores) {
            device->destroySemaphore(semaphore);
        }

        for (auto& semaphore : imgAvailable) {
            device->destroySemaphore(semaphore);
        }

        for (auto& fence : cmdBufFences) {
            device->destroyFence(fence);
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
        loadedMeshes.clear();

        delete shadowCascadePass;

        delete imguiImage;
        delete shadowmapImage;
        delete finalPrePresent;

        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
            delete shadowImages[i];
        }

        if (enableVR)
            delete finalPrePresentR;

        materialUB.destroy();

#ifndef NDEBUG
        char* statsString;
        vmaBuildStatsString(allocator, &statsString, true);

        FILE* file = fopen("memory_shutdown.json", "w");
        fwrite(statsString, strlen(statsString), 1, file);
        fclose(file);

        vmaFreeStatsString(allocator, statsString);
#endif
        vmaDestroyAllocator(allocator);

        dbgCallback.reset();

        swapchain.reset();

        instance->destroySurfaceKHR(surface);
        logMsg(WELogCategoryRender, "Renderer destroyed.");
    }
}
