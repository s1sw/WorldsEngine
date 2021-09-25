#define _CRT_SECURE_NO_WARNINGS
#define VMA_IMPLEMENTATION
#include "../Libs/volk.h"
#include "vku/vku.hpp"
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
#include "RenderInternal.hpp"
#ifdef RDOC
#include "renderdoc_app.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "../Util/EnumUtil.hpp"
#include "vku/DeviceMaker.hpp"
#include "vku/InstanceMaker.hpp"

using namespace worlds;

const bool vrValidationLayers = false;

VkPhysicalDeviceProperties getPhysicalDeviceProperties(VkPhysicalDevice pd) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pd, &props);
    return props;
}

std::vector<VkQueueFamilyProperties> getQueueFamilies(VkPhysicalDevice pd) {
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> qprops;
    qprops.resize(queueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, qprops.data());

    return qprops;
}

VkQueue getQueue(VkDevice device, uint32_t qfi) {
    VkQueue queue;
    vkGetDeviceQueue(device, qfi, 0, &queue);
    return queue;
}

uint32_t findPresentQueue(VkPhysicalDevice pd, VkSurfaceKHR surface) {
    std::vector<VkQueueFamilyProperties> qprops = getQueueFamilies(pd);

    for (uint32_t qi = 0; qi != qprops.size(); ++qi) {
        auto& qprop = qprops[qi];

        VkBool32 supported;
        vkGetPhysicalDeviceSurfaceSupportKHR(pd, qi, surface, &supported);
        if (supported && enumHasFlag(qprop.queueFlags, VK_QUEUE_GRAPHICS_BIT)) {
            return qi;
        }
    }

    return ~0u;
}

RenderTexture* VKRenderer::createRTResource(RTResourceCreateInfo resourceCreateInfo, const char* debugName) {
    return new RenderTexture{ &handles, resourceCreateInfo, debugName };
}

void VKRenderer::createSwapchain(VkSwapchainKHR oldSwapchain) {
    bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN;
    VkPresentModeKHR presentMode = (useVsync && !enableVR) ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    QueueFamilyIndices qfi{ graphicsQueueFamilyIdx, presentQueueFamilyIdx };
    swapchain = new Swapchain(physicalDevice, device, surface, qfi, fullscreen, oldSwapchain, presentMode);
    swapchain->getSize(&width, &height);

    vku::executeImmediately(device, commandPool, getQueue(device, graphicsQueueFamilyIdx), [this](VkCommandBuffer cb) {
        for (VkImage img : swapchain->images)
            vku::transitionLayout(cb, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VkAccessFlags{}, VK_ACCESS_MEMORY_READ_BIT);
        });
}

void VKRenderer::createFramebuffers() {
    for (size_t i = 0; i != swapchain->imageViews.size(); i++) {
        VkImageView attachments[1] = { swapchain->imageViews[i] };
        VkFramebufferCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.attachmentCount = 1;
        fci.pAttachments = attachments;
        fci.width = width;
        fci.height = height;
        fci.renderPass = irp->getRenderPass();
        fci.layers = 1;

        VkFramebuffer framebuffer;
        VKCHECK(vkCreateFramebuffer(device, &fci, nullptr, &framebuffer));
        framebuffers.push_back(framebuffer);
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

    for (auto& e : instanceExtensions) {
        logVrb(WELogCategoryRender, "activating extension: %s", e.c_str());
        instanceMaker.extension(e.c_str());
    }

#ifndef NDEBUG
    if (!enableVR || vrValidationLayers) {
        logVrb(WELogCategoryRender, "Activating validation layers");
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

    instance = instanceMaker.create();
}

void logPhysDevInfo(const VkPhysicalDevice& physicalDevice) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    VkPhysicalDeviceProperties physDevProps;

    vkGetPhysicalDeviceProperties(physicalDevice, &physDevProps);
    logVrb(worlds::WELogCategoryRender, "Physical device:\n");
    logVrb(worlds::WELogCategoryRender, "\t-Name: %s", physDevProps.deviceName);
    logVrb(worlds::WELogCategoryRender, "\t-ID: %u", physDevProps.deviceID);
    logVrb(worlds::WELogCategoryRender, "\t-Vendor ID: %u", physDevProps.vendorID);
    logVrb(worlds::WELogCategoryRender, "\t-Device Type: %s", vku::toString(physDevProps.deviceType));
    logVrb(worlds::WELogCategoryRender, "\t-Driver Version: %u", physDevProps.driverVersion);
    logVrb(worlds::WELogCategoryRender, "\t-Memory heap count: %u", memProps.memoryHeapCount);
    logVrb(worlds::WELogCategoryRender, "\t-Memory type count: %u", memProps.memoryTypeCount);

    VkDeviceSize totalVram = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
        auto& heap = memProps.memoryHeaps[i];
        totalVram += heap.size;
        logVrb(worlds::WELogCategoryRender, "Heap %i: %hu MB", i, heap.size / 1024 / 1024);
    }

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        auto& memType = memProps.memoryTypes[i];
        const char* str = vku::toString(memType.propertyFlags);
        logVrb(worlds::WELogCategoryRender, "Memory type for heap %i: %s", memType.heapIndex, str);
        free((char*)str);
    }

    logVrb(worlds::WELogCategoryRender, "Approx. %hu MB total accessible graphics memory (NOT VRAM!)", totalVram / 1024 / 1024);
}

bool checkPhysicalDeviceFeatures(const VkPhysicalDevice& physDev) {
    VkPhysicalDeviceFeatures supportedFeatures{};
    vkGetPhysicalDeviceFeatures(physDev, &supportedFeatures);

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

    VkPhysicalDeviceFeatures2 supportedFeatures2{};
    supportedFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    VkPhysicalDeviceVulkan11Features supportedVk11Features{};
    supportedVk11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    VkPhysicalDeviceVulkan12Features supportedVk12Features{};
    supportedVk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    supportedFeatures2.pNext = &supportedVk11Features;
    supportedVk11Features.pNext = &supportedVk12Features;
    vkGetPhysicalDeviceFeatures2(physDev, &supportedFeatures2);

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

bool isDeviceBetter(VkPhysicalDevice a, VkPhysicalDevice b) {
    VkPhysicalDeviceProperties aProps;
    VkPhysicalDeviceProperties bProps;
    vkGetPhysicalDeviceProperties(a, &aProps);
    vkGetPhysicalDeviceProperties(b, &bProps);

    if (bProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        aProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        return true;
    } else if (aProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        bProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        return false;
    }

    return aProps.deviceID < bProps.deviceID;
}

VkPhysicalDevice pickPhysicalDevice(std::vector<VkPhysicalDevice>& physicalDevices) {
    std::sort(physicalDevices.begin(), physicalDevices.end(), isDeviceBetter);

    return physicalDevices[0];
}

VKRenderer::VKRenderer(const RendererInitInfo& initInfo, bool* success)
    : finalPrePresent(nullptr)
    , leftEye(nullptr)
    , rightEye(nullptr)
    , shadowmapImage(nullptr)
    , imguiImage(nullptr)
    , window(initInfo.window)
    , shadowmapRes(4096)
    , enableVR(initInfo.enableVR)
    , irp(nullptr)
    , vrPredictAmount(0.033f)
    , clearMaterialIndices(false)
    , useVsync(true)
    , enablePicking(initInfo.enablePicking)
    , frameIdx(0)
    , lastFrameIdx(0) {
    maxFramesInFlight = 2;
    msaaSamples = VK_SAMPLE_COUNT_2_BIT;
    numMSAASamples = 2;

#ifdef RDOC
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdocApi);
        assert(ret == 1);
    } else {
        rdocApi = nullptr;
    }
#endif

    if (volkInitialize() != VK_SUCCESS) {
        fatalErr("Couldn't find Vulkan.");
    }

    createInstance(initInfo);
    volkLoadInstance(instance);

#ifndef NDEBUG
    if (!enableVR || vrValidationLayers)
        dbgCallback = vku::DebugCallback(instance);
#endif
    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

    std::vector<VkPhysicalDevice> physDevs;
    physDevs.resize(physicalDeviceCount);
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physDevs.data());

    physicalDevice = pickPhysicalDevice(physDevs);

    logPhysDevInfo(physicalDevice);

    std::vector<VkQueueFamilyProperties> qprops = getQueueFamilies(physicalDevice);

    const auto badQueue = ~(uint32_t)0;
    graphicsQueueFamilyIdx = badQueue;
    VkQueueFlags search = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

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
        if ((qprop.queueFlags & (VK_QUEUE_COMPUTE_BIT)) == VK_QUEUE_COMPUTE_BIT && i != graphicsQueueFamilyIdx) {
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

    dm.extension(VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME);
#if TRACY_ENABLE
    dm.extension(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
#endif

    if (!checkPhysicalDeviceFeatures(physicalDevice)) {
        *success = false;
        return;
    }

    VkPhysicalDeviceFeatures features{};
    features.shaderStorageImageMultisample = true;
    features.fragmentStoresAndAtomics = true;
    features.fillModeNonSolid = true;
    features.wideLines = true;
    features.samplerAnisotropy = true;
    dm.setFeatures(features);

    VkPhysicalDeviceVulkan11Features vk11Features{};
    vk11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vk11Features.multiview = true;
    dm.setPNext(&vk11Features);

    VkPhysicalDeviceVulkan12Features vk12Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    vk12Features.timelineSemaphore = true;
    vk12Features.descriptorBindingPartiallyBound = true;
    vk12Features.runtimeDescriptorArray = true;
    vk12Features.imagelessFramebuffer = true;

    vk11Features.pNext = &vk12Features;

    device = dm.create(physicalDevice);

    ShaderCache::setDevice(device);

    VmaAllocatorCreateInfo allocatorCreateInfo{};
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.frameInUseCount = 0;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorCreateInfo.flags = 0;
    vmaCreateAllocator(&allocatorCreateInfo, &allocator);

    VkPipelineCacheCreateInfo pipelineCacheInfo{};
    PipelineCacheSerializer::loadPipelineCache(getPhysicalDeviceProperties(physicalDevice), pipelineCacheInfo);

    VKCHECK(vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr, &pipelineCache));
    std::free((void*)pipelineCacheInfo.pInitialData);

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024);
    poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024);
    poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024);

    // Create an arbitrary number of descriptors in a pool.
    // Allow the descriptors to be freed, possibly not optimal behaviour.
    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolInfo.maxSets = 256;
    descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    descriptorPoolInfo.pPoolSizes = poolSizes.data();

    VKCHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

    // Create surface and find presentation queue
    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, instance, &surface);

    this->surface = surface;
    presentQueueFamilyIdx = findPresentQueue(physicalDevice, surface);

    // Command pool
    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = graphicsQueueFamilyIdx;

    VKCHECK(vkCreateCommandPool(device, &cpci, nullptr, &commandPool));
    vku::setObjectName(device, (uint64_t)commandPool, VK_OBJECT_TYPE_COMMAND_POOL, "Main Command Pool");

    createSwapchain(nullptr);

    if (initInfo.activeVrApi == VrApi::OpenVR) {
        OpenVRInterface* vrInterface = static_cast<OpenVRInterface*>(initInfo.vrInterface);
        vrInterface->getRenderResolution(&vrWidth, &vrHeight);
    }

    VkPhysicalDeviceProperties physDevProps = getPhysicalDeviceProperties(physicalDevice);

    VKVendor vendor = VKVendor::Other;

    switch (physDevProps.vendorID) {
    case 0x1002:
        vendor = VKVendor::AMD;
        break;
    case 0x10DE:
        vendor = VKVendor::Nvidia;
        break;
    case 0x8086:
        vendor = VKVendor::Intel;
        break;
    }

    handles = VulkanHandles{
        vendor,
        physicalDevice,
        device,
        pipelineCache,
        descriptorPool,
        commandPool,
        instance,
        allocator,
        graphicsQueueFamilyIdx,
        GraphicsSettings {
            numMSAASamples,
            (int)shadowmapRes,
            enableVR
        },
        width, height,
        vrWidth, vrHeight
    };

    auto vkCtx = std::make_shared<VulkanHandles>(handles);

    cubemapConvoluter = std::make_shared<CubemapConvoluter>(vkCtx);

    texSlots = std::make_unique<TextureSlots>(vkCtx);
    matSlots = std::make_unique<MaterialSlots>(vkCtx, *texSlots);
    cubemapSlots = std::make_unique<CubemapSlots>(vkCtx, cubemapConvoluter);

    VkImageCreateInfo brdfLutIci{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        VkImageCreateFlags{},
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16_SFLOAT,
        VkExtent3D { 256, 256, 1 }, 1, 1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SHARING_MODE_EXCLUSIVE, graphicsQueueFamilyIdx
    };

    brdfLut = vku::GenericImage{ device, allocator, brdfLutIci, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, false, "BRDF LUT" };

    vku::executeImmediately(device, commandPool, getQueue(device, graphicsQueueFamilyIdx), [&](auto cb) {
        brdfLut.setLayout(cb, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        });

    BRDFLUTRenderer brdfLutRenderer{ *vkCtx };
    brdfLutRenderer.render(*vkCtx, brdfLut);

    VkImageCreateInfo shadowmapIci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    shadowmapIci.imageType = VK_IMAGE_TYPE_2D;
    shadowmapIci.extent = VkExtent3D{ shadowmapRes, shadowmapRes, 1 };
    shadowmapIci.arrayLayers = 3;
    shadowmapIci.mipLevels = 1;
    shadowmapIci.format = VK_FORMAT_D32_SFLOAT;
    shadowmapIci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    shadowmapIci.samples = VK_SAMPLE_COUNT_1_BIT;
    shadowmapIci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    RTResourceCreateInfo shadowmapCreateInfo{ shadowmapIci, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_ASPECT_DEPTH_BIT };
    shadowmapImage = createRTResource(shadowmapCreateInfo, "Shadowmap Image");
    shadowmapIci.arrayLayers = 1;

    shadowmapIci.extent = VkExtent3D{ 512, 512, 1 };
    for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
        RTResourceCreateInfo shadowCreateInfo{ shadowmapIci, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT };
        shadowImages[i] = createRTResource(shadowCreateInfo, ("Shadow Image " + std::to_string(i)).c_str());
    }

    vku::executeImmediately(device, commandPool, getQueue(device, graphicsQueueFamilyIdx), [&](auto cb) {
        shadowmapImage->image.setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
            shadowImages[i]->image.setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        }
        });

    createSCDependents();

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = commandPool;
    cbai.commandBufferCount = maxFramesInFlight;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    cmdBufs.resize(maxFramesInFlight);

    VKCHECK(vkAllocateCommandBuffers(device, &cbai, cmdBufs.data()));

    cmdBufFences.resize(maxFramesInFlight);
    cmdBufferSemaphores.resize(maxFramesInFlight);
    imgAvailable.resize(maxFramesInFlight);

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        std::string cmdBufName = "Command Buffer ";
        cmdBufName += std::to_string(i);

        vku::setObjectName(device, (uint64_t)cmdBufs[i], VK_OBJECT_TYPE_COMMAND_BUFFER, cmdBufName.c_str());
        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VKCHECK(vkCreateFence(device, &fci, nullptr, &cmdBufFences[i]));

        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VKCHECK(vkCreateSemaphore(device, &sci, nullptr, &cmdBufferSemaphores[i]));
        VKCHECK(vkCreateSemaphore(device, &sci, nullptr, &imgAvailable[i]));
    }
    imgFences.resize(cmdBufs.size());

    timestampPeriod = physDevProps.limits.timestampPeriod;

    VkQueryPoolCreateInfo qpci{};
    qpci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qpci.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qpci.queryCount = 2 * maxFramesInFlight;

    VKCHECK(vkCreateQueryPool(device, &qpci, nullptr, &queryPool));

    *success = true;
#ifdef TRACY_ENABLE
    for (auto& cmdBuf : cmdBufs) {
        VkQueue queue;
        vkGetDeviceQueue(device, graphicsQueueFamilyIdx, 0, &queue);
        tracyContexts.push_back(tracy::CreateVkContext(physicalDevice, device, queue, cmdBuf, vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, vkGetCalibratedTimestampsEXT));
    }
#endif

    if (enableVR) {
        if (initInfo.activeVrApi == VrApi::OpenVR) {
            vr::VRCompositor()->SetExplicitTimingMode(vr::EVRCompositorTimingMode::VRCompositorTimingMode_Explicit_ApplicationPerformsPostPresentHandoff);
        }

        vrInterface = initInfo.vrInterface;
        vrApi = initInfo.activeVrApi;
    }

    // Load cubemap for the sky
    cubemapSlots->loadOrGet(AssetDB::pathToId("envmap_miramar/miramar.json"));

    g_console->registerCommand([&](void*, const char* arg) {
        numMSAASamples = std::atoi(arg);
        // The sample count flags are actually identical to the number of samples
        msaaSamples = (VkSampleCountFlagBits)numMSAASamples;
        handles.graphicsSettings.msaaLevel = numMSAASamples;
        recreateSwapchain();
        }, "r_setMSAASamples", "Sets the number of MSAA samples.", nullptr);

    g_console->registerCommand([&](void*, const char*) {
        recreateSwapchain();
        }, "r_recreateSwapchain", "", nullptr);

    g_console->registerCommand([&](void*, const char*) {
        char* statsString;
        vmaBuildStatsString(allocator, &statsString, true);
        logVrb(WELogCategoryRender, "%s", statsString);
        auto file = PHYSFS_openWrite("memory.json");
        PHYSFS_writeBytes(file, statsString, strlen(statsString));
        PHYSFS_close(file);
        vmaFreeStatsString(allocator, statsString);
        }, "r_printAllocInfo", "", nullptr);

    g_console->registerCommand([&](void*, const char* arg) {
        vkDeviceWaitIdle(device);
        shadowmapRes = std::atoi(arg);
        handles.graphicsSettings.shadowmapRes = shadowmapRes;
        delete shadowmapImage;
        VkImageCreateInfo shadowmapIci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        shadowmapIci.imageType = VK_IMAGE_TYPE_2D;
        shadowmapIci.extent = VkExtent3D{ shadowmapRes, shadowmapRes, 1 };
        shadowmapIci.arrayLayers = 3;
        shadowmapIci.mipLevels = 1;
        shadowmapIci.format = VK_FORMAT_D16_UNORM;
        shadowmapIci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        shadowmapIci.samples = VK_SAMPLE_COUNT_1_BIT;
        shadowmapIci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        RTResourceCreateInfo shadowmapCreateInfo{ shadowmapIci, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_ASPECT_DEPTH_BIT };
        shadowmapImage = createRTResource(shadowmapCreateInfo, "Shadowmap Image");
        delete shadowCascadePass;
        shadowCascadePass = new ShadowCascadePass(&handles, shadowmapImage);
        shadowCascadePass->setup();

        for (VKRTTPass* rttPass : rttPasses) {
            rttPass->prp->reuploadDescriptors();
        }
        }, "r_setCSMResolution", "Sets the resolution of the cascaded shadow map.");

    shadowCascadePass = new ShadowCascadePass(&handles, shadowmapImage);
    shadowCascadePass->setup();

    additionalShadowsPass = new AdditionalShadowsPass(&handles);
    additionalShadowsPass->setup();

    materialUB = vku::GenericBuffer(
        device, allocator,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof(MaterialsUB), VMA_MEMORY_USAGE_GPU_ONLY, "Materials");

    vpBuffer = vku::GenericBuffer(
        device, allocator,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof(MultiVP), VMA_MEMORY_USAGE_GPU_ONLY, "VP Buffer");

    MaterialsUB materials;
    materialUB.upload(device, commandPool, getQueue(device, graphicsQueueFamilyIdx), &materials, sizeof(materials));
}

// Quite a lot of resources are dependent on either the number of images
// there are in the swap chain or the swapchain itself, so they need to be
// recreated whenever the swap chain changes.
void VKRenderer::createSCDependents() {
    delete imguiImage;
    delete finalPrePresent;

    if (leftEye) {
        delete leftEye;
        delete rightEye;
    }

    if (irp == nullptr) {
        irp = new ImGuiRenderPass(&handles, *swapchain);
        irp->setup();
    }

    createFramebuffers();

    VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent = VkExtent3D{ width, height, 1 };
    ici.arrayLayers = 1;
    ici.mipLevels = 1;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT;

    RTResourceCreateInfo imguiImageCreateInfo{ ici, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT };
    imguiImage = createRTResource(imguiImageCreateInfo, "ImGui Image");

    ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    RTResourceCreateInfo finalPrePresentCI{ ici, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT };

    finalPrePresent = createRTResource(finalPrePresentCI, "Final Pre-Present");

    if (enableVR) {
        ici.extent = VkExtent3D{ vrWidth, vrHeight, 1 };
        RTResourceCreateInfo eyeCreateInfo{ ici, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT };
        leftEye = createRTResource(eyeCreateInfo, "Left Eye");
        rightEye = createRTResource(eyeCreateInfo, "Right Eye");
    }

    vku::executeImmediately(device, commandPool, getQueue(device, graphicsQueueFamilyIdx), [&](VkCommandBuffer cmdBuf) {
        finalPrePresent->image.setLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        });

    for (auto& p : rttPasses) {
        if (p->outputToScreen) {
            p->isValid = false;
        }
    }

    imgFences.clear();
    imgFences.resize(swapchain->images.size());

    for (auto& s : imgAvailable) {
        vkDestroySemaphore(device, s, nullptr);
    }

    imgAvailable.clear();
    imgAvailable.resize(cmdBufs.size());

    for (size_t i = 0; i < cmdBufs.size(); i++) {
        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VKCHECK(vkCreateSemaphore(device, &sci, nullptr, &imgAvailable[i]));
    }
}

VkSurfaceCapabilitiesKHR getSurfaceCaps(VkPhysicalDevice pd, VkSurfaceKHR surf) {
    VkSurfaceCapabilitiesKHR surfCaps;
    VKCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surf, &surfCaps));
    return surfCaps;
}

void VKRenderer::recreateSwapchain() {
    // Wait for current frame to finish
    vkDeviceWaitIdle(device);

    // Check width/height - if it's 0, just ignore it
    auto surfaceCaps = getSurfaceCaps(physicalDevice, surface);

    if (surfaceCaps.currentExtent.width == 0 || surfaceCaps.currentExtent.height == 0) {
        logVrb(WELogCategoryRender, "Ignoring resize with 0 width or height");
        isMinimised = true;

        while (isMinimised) {
            auto surfaceCaps = getSurfaceCaps(physicalDevice, surface);
            isMinimised = surfaceCaps.currentExtent.width == 0 || surfaceCaps.currentExtent.height == 0;
            SDL_PumpEvents();
            SDL_Delay(50);
        }

        recreateSwapchain();
        return;
    }

    isMinimised = false;

    logVrb(WELogCategoryRender, "Recreating swapchain: New surface size is %ix%i",
        surfaceCaps.currentExtent.width, surfaceCaps.currentExtent.height);

    if (surfaceCaps.currentExtent.width > 0 && surfaceCaps.currentExtent.height > 0) {
        width = surfaceCaps.currentExtent.width;
        height = surfaceCaps.currentExtent.height;
    }

    if (surfaceCaps.currentExtent.width == 0 || surfaceCaps.currentExtent.height == 0) {
        isMinimised = true;
        return;
    } else {
        isMinimised = false;
    }

    delete swapchain;
    for (VkFramebuffer fb : framebuffers)
        vkDestroyFramebuffer(device, fb, nullptr);

    framebuffers.clear();

    createSwapchain(nullptr);
    createSCDependents();

    swapchainRecreated = true;
}

void VKRenderer::presentNothing(uint32_t imageIndex) {
    VkSemaphore imgSemaphore = imgAvailable[frameIdx];
    VkSemaphore cmdBufSemaphore = cmdBufferSemaphores[frameIdx];

    VkPresentInfoKHR presentInfo;
    VkSwapchainKHR cSwapchain = swapchain->getSwapchain();
    presentInfo.pSwapchains = &cSwapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imageIndex;

    presentInfo.pWaitSemaphores = &cmdBufSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    auto& cmdBuf = cmdBufs[frameIdx];
    vku::beginCommandBuffer(cmdBuf);
    VKCHECK(vkEndCommandBuffer(cmdBuf));

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imgSemaphore;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &cmdBufferSemaphores[frameIdx];
    submitInfo.pCommandBuffers = &cmdBuf;
    submitInfo.commandBufferCount = 1;
    VkQueue q = getQueue(device, presentQueueFamilyIdx);
    vkQueueSubmit(q, 1, &submitInfo, VK_NULL_HANDLE);

    auto presentResult = vkQueuePresentKHR(q, &presentInfo);
    if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR)
        fatalErr("Present failed!");
}

void imageBarrier(VkCommandBuffer& cb, VkImage image, VkImageLayout layout,
    VkAccessFlags srcMask, VkAccessFlags dstMask,
    VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t numLayers = 1) {
    VkImageMemoryBarrier imageMemoryBarriers{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.oldLayout = layout;
    imageMemoryBarriers.newLayout = layout;
    imageMemoryBarriers.image = image;
    imageMemoryBarriers.subresourceRange = { aspectMask, 0, 1, 0, numLayers };

    // Put barrier on top

    imageMemoryBarriers.srcAccessMask = srcMask;
    imageMemoryBarriers.dstAccessMask = dstMask;

    vkCmdPipelineBarrier(cb, srcStageMask, dstStageMask, VK_DEPENDENCY_BY_REGION_BIT,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarriers);
}

vku::ShaderModule VKRenderer::loadShaderAsset(AssetID id) {
    ZoneScoped;
    PHYSFS_File* file = AssetDB::openAssetFileRead(id);
    size_t size = PHYSFS_fileLength(file);
    void* buffer = std::malloc(size);

    size_t readBytes = PHYSFS_readBytes(file, buffer, size);
    assert(readBytes == size);
    PHYSFS_close(file);

    vku::ShaderModule sm{ device, static_cast<uint32_t*>(buffer), readBytes };
    std::free(buffer);
    return sm;
}

void VKRenderer::submitToOpenVR() {
    // Submit to SteamVR
    vr::VRTextureBounds_t bounds{
        .uMin = 0.0f,
        .vMin = 0.0f,
        .uMax = 1.0f,
        .vMax = 1.0f
    };

    VkImage vkImg = leftEye->image.image();

    vr::VRVulkanTextureData_t vulkanData{
        .m_nImage = (uint64_t)vkImg,
        .m_pDevice = device,
        .m_pPhysicalDevice = (VkPhysicalDevice_T*)physicalDevice,
        .m_pInstance = instance,
        .m_pQueue = getQueue(device, graphicsQueueFamilyIdx),
        .m_nQueueFamilyIndex = graphicsQueueFamilyIdx,
        .m_nWidth = vrWidth,
        .m_nHeight = vrHeight,
        .m_nFormat = VK_FORMAT_R8G8B8A8_UNORM,
        .m_nSampleCount = 1
    };

    // Image submission with validation layers turned on causes a crash
    // If we really want the validation layers, don't submit anything
    if (!vrValidationLayers) {
        vr::Texture_t texture = { &vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Auto };
        vr::VRCompositor()->Submit(vr::Eye_Left, &texture, &bounds);

        vulkanData.m_nImage = (uint64_t)(VkImage)rightEye->image.image();
        vr::VRCompositor()->Submit(vr::Eye_Right, &texture, &bounds);
    }
}

void VKRenderer::uploadSceneAssets(entt::registry& reg) {
    ZoneScoped;
    bool reuploadMats = false;

    std::unordered_set<AssetID> uploadMats;
    std::unordered_set<AssetID> uploadMeshes;

    // Upload any necessary materials + meshes
    reg.view<WorldObject>().each([&](WorldObject& wo) {
        for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
            if (!wo.presentMaterials[i]) continue;

            if (wo.materialIdx[i] == ~0u) {
                reuploadMats = true;
                uploadMats.insert(wo.materials[i]);
            }
        }

        if (loadedMeshes.find(wo.mesh) == loadedMeshes.end()) {
            uploadMeshes.insert(wo.mesh);
        }
        });

    if (uploadMats.size()) {
        JobList& jl = g_jobSys->getFreeJobList();
        jl.begin();

        int i = 0;
        for (AssetID id : uploadMats) {
            Job j{
                [id, this] {
                    matSlots->loadOrGet(id);
                }
            };
            jl.addJob(std::move(j));
            i++;
        }

        jl.end();
        g_jobSys->signalJobListAvailable();
        jl.wait();
    }

    reg.view<WorldObject>().each([&](WorldObject& wo) {
        for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
            if (!wo.presentMaterials[i]) continue;

            if (wo.materialIdx[i] == ~0u) {
                wo.materialIdx[i] = matSlots->loadOrGet(wo.materials[i]);
            }
        }
        });

    for (AssetID id : uploadMeshes) {
        preloadMesh(id);
    }

    reg.view<WorldCubemap>().each([&](WorldCubemap& wc) {
        if (!cubemapSlots->isLoaded(wc.cubemapId)) {
            cubemapSlots->loadOrGet(wc.cubemapId);
            reuploadMats = true;
        }
        });

    if (reuploadMats)
        reuploadMaterials();
}

glm::mat4 VKRenderer::getCascadeMatrix(bool forVr, Camera cam, glm::vec3 lightDir, glm::mat4 frustumMatrix, float& texelsPerUnit) {
    glm::mat4 view;

    if (!forVr) {
        view = cam.getViewMatrix();
    } else {
        view = glm::inverse(vrInterface->getHeadTransform(vrPredictAmount)) * cam.getViewMatrix();
    }

    glm::mat4 vpInv = glm::inverse(frustumMatrix * view);

    glm::vec3 frustumCorners[8] = {
        glm::vec3(-1.0f,  1.0f, -1.0f),
        glm::vec3(1.0f,  1.0f, -1.0f),
        glm::vec3(1.0f, -1.0f, -1.0f),
        glm::vec3(-1.0f, -1.0f, -1.0f),
        glm::vec3(-1.0f,  1.0f,  1.0f),
        glm::vec3(1.0f,  1.0f,  1.0f),
        glm::vec3(1.0f, -1.0f,  1.0f),
        glm::vec3(-1.0f, -1.0f,  1.0f),
    };

    for (int i = 0; i < 8; i++) {
        glm::vec4 transformed = vpInv * glm::vec4{ frustumCorners[i], 1.0f };
        transformed /= transformed.w;
        frustumCorners[i] = transformed;
    }

    glm::vec3 center{ 0.0f };

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

    glm::mat4 scaleMatrix = glm::scale(glm::mat4{ 1.0f }, glm::vec3{ texelsPerUnit });

    glm::mat4 lookAt = glm::lookAt(glm::vec3{ 0.0f }, lightDir, glm::vec3{ 0.0f, 1.0f, 0.0f });
    lookAt *= scaleMatrix;

    glm::mat4 lookAtInv = glm::inverse(lookAt);

    center = lookAt * glm::vec4{ center, 1.0f };
    center = glm::floor(center);
    center = lookAtInv * glm::vec4{ center, 1.0f };

    glm::vec3 eye = center + (lightDir * diameter);

    glm::mat4 viewMat = glm::lookAt(eye, center, glm::vec3{ 0.0f, 1.0f, 0.0f });
    glm::mat4 projMat = glm::orthoZO(-radius, radius, -radius, radius, radius * 12.0f, -radius * 12.0f);

    return projMat * viewMat;
}

worlds::ConVar doGTAO{ "r_doGTAO", "0" };

void VKRenderer::calculateCascadeMatrices(bool forVr, entt::registry& world, Camera& cam, RenderContext& rCtx) {
    world.view<WorldLight, Transform>().each([&](auto, WorldLight& l, Transform& transform) {
        glm::vec3 lightForward = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        if (l.type == LightType::Directional) {
            glm::mat4 frustumMatrices[3];
            float aspect = (float)rCtx.passWidth / (float)rCtx.passHeight;
            // frustum 0: near -> 20m
            // frustum 1: 20m  -> 125m
            // frustum 2: 125m -> 250m
            float splits[4] = { 0.1f, 15.0f, 45.0f, 105.0f };
            if (!rCtx.passSettings.enableVR) {
                for (int i = 1; i < 4; i++) {
                    frustumMatrices[i - 1] = glm::perspective(
                        cam.verticalFOV, aspect,
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
                rCtx.cascadeInfo.matrices[i] =
                    getCascadeMatrix(
                        forVr,
                        cam, lightForward,
                        frustumMatrices[i], rCtx.cascadeInfo.texelsPerUnit[i]
                    );
            }
        }
        });
}

void VKRenderer::writeCmdBuf(VkCommandBuffer cmdBuf, uint32_t imageIndex, Camera& cam, entt::registry& reg) {
    ZoneScoped;

    vku::beginCommandBuffer(cmdBuf, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    texSlots->frameStarted = true;

    vkCmdResetQueryPool(cmdBuf, queryPool, 2 * frameIdx, 2);
    vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, frameIdx * 2);

    texSlots->setUploadCommandBuffer(cmdBuf, frameIdx);

    if (clearMaterialIndices) {
        reg.view<WorldObject>().each([](entt::entity, WorldObject& wo) {
            memset(wo.materialIdx, ~0u, sizeof(wo.materialIdx));
            });
        clearMaterialIndices = false;
    }

    uploadSceneAssets(reg);

    finalPrePresent->image.setLayout(cmdBuf,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    std::sort(rttPasses.begin(), rttPasses.end(), [](VKRTTPass* a, VKRTTPass* b) {
        return a->drawSortKey < b->drawSortKey;
        });

    RenderContext rCtx{
        .resources = getResources(),
        .cascadeInfo = {},
        .debugContext = RenderDebugContext {
            .stats = &dbgStats
#ifdef TRACY_ENABLE
            , .tracyContexts = &tracyContexts
#endif
        },
        .passSettings = PassSettings {
            .enableVR = false,
            .enableShadows = true
        },
        .registry = reg,
        .cmdBuf = cmdBuf,
        .imageIndex = frameIdx,
        .maxSimultaneousFrames = maxFramesInFlight
    };

    additionalShadowsPass->prePass(rCtx);
    additionalShadowsPass->execute(rCtx);

    int numActivePasses = 0;
    bool lastPassIsVr = false;
    for (auto& p : rttPasses) {
        if (!p->active || !p->isValid) continue;
        numActivePasses++;

        if (!p->outputToScreen) {
            p->sdrFinalTarget->image.setLayout(cmdBuf,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        }

        bool nullCam = p->cam == nullptr;

        if (nullCam)
            p->cam = &cam;

        p->writeCmds(frameIdx, cmdBuf, reg);

        if (nullCam)
            p->cam = nullptr;

        if (!p->outputToScreen) {
            p->sdrFinalTarget->image.setLayout(cmdBuf,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT);
        }
        lastPassIsVr = p->isVr;
    }
    dbgStats.numActiveRTTPasses = numActivePasses;
    dbgStats.numRTTPasses = rttPasses.size();

    vku::transitionLayout(cmdBuf, swapchain->images[imageIndex],
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);


    if (enableVR) {
        leftEye->image.setLayout(cmdBuf,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT);

        rightEye->image.setLayout(cmdBuf,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT);
    }

    finalPrePresent->image.setLayout(cmdBuf,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);

    //cmdBuf->clearColorImage(
    //    swapchain->images[imageIndex],
    //    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    //    VkClearColorValue{ std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } },
    //    VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    //);

    if (!lastPassIsVr) {
        VkImageBlit imageBlit{};
        imageBlit.srcOffsets[1] = imageBlit.dstOffsets[1] = VkOffset3D{ (int)width, (int)height, 1 };
        imageBlit.dstSubresource = imageBlit.srcSubresource = VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        vkCmdBlitImage(cmdBuf, finalPrePresent->image.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain->images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_NEAREST);
    } else {
        // Calculate the best crop for the current window size against the VR render target
        //float scaleFac = glm::min((float)windowSize.x / renderWidth, (float)windowSize.y / renderHeight);
        float aspect = (float)windowSize.y / (float)windowSize.x;
        float croppedHeight = aspect * vrWidth;

        glm::vec2 srcCorner0(0.0f, vrHeight / 2.0f - croppedHeight / 2.0f);
        glm::vec2 srcCorner1(vrWidth, vrHeight / 2.0f + croppedHeight / 2.0f);

        VkImageBlit imageBlit{};
        imageBlit.srcOffsets[0] = VkOffset3D{ (int)srcCorner0.x, (int)srcCorner0.y, 0 };
        imageBlit.srcOffsets[1] = VkOffset3D{ (int)srcCorner1.x, (int)srcCorner1.y, 1 };
        imageBlit.dstOffsets[1] = VkOffset3D{ (int)windowSize.x, (int)windowSize.y, 1 };
        imageBlit.dstSubresource = imageBlit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

        vkCmdBlitImage(cmdBuf, leftEye->image.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain->images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
    }

    vku::transitionLayout(cmdBuf, swapchain->images[imageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);

    irp->execute(cmdBuf, width, height, framebuffers[imageIndex]);

    ::imageBarrier(cmdBuf, swapchain->images[imageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_MEMORY_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1 + (frameIdx * 2));
#ifdef TRACY_ENABLE
    TracyVkCollect(tracyContexts[imageIndex], cmdBuf);
#endif
    texSlots->frameStarted = false;
    VKCHECK(vkEndCommandBuffer(cmdBuf));
}

void VKRenderer::reuploadMaterials() {
    ZoneScoped;
    materialUB.upload(device, commandPool, getQueue(device, graphicsQueueFamilyIdx), matSlots->getSlots(), sizeof(PackedMaterial) * 256);

    for (auto& pass : rttPasses) {
        pass->prp->reuploadDescriptors();
    }
}

ConVar showSlotDebug{ "r_debugSlots", "0", "Shows a window for debugging resource slots." };

void VKRenderer::frame(Camera& cam, entt::registry& reg) {
    ZoneScoped;

    if (vrApi == VrApi::OpenVR) {
        ZoneScopedN("WaitGetPoses");
        vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);
    }

    if (showSlotDebug.getInt()) {
        if (ImGui::Begin("Render Slot Debug")) {
            if (ImGui::CollapsingHeader("Cubemap Slots")) {
                for (uint32_t i = 0; i < NUM_CUBEMAP_SLOTS; i++) {
                    if (cubemapSlots->isSlotPresent(i)) {
                        ImGui::Text("Slot %u: %s", i, AssetDB::idToPath(cubemapSlots->getKeyForSlot(i)).c_str());
                    }
                }
            }

            if (ImGui::CollapsingHeader("Texture Slots")) {
                for (uint32_t i = 0; i < NUM_TEX_SLOTS; i++) {
                    if (texSlots->isSlotPresent(i)) {
                        ImGui::Text("Slot %u: %s", i, AssetDB::idToPath(texSlots->getKeyForSlot(i)).c_str());
                    }
                }
            }

            if (ImGui::CollapsingHeader("Material Slots")) {
                for (uint32_t i = 0; i < NUM_MAT_SLOTS; i++) {
                    if (matSlots->isSlotPresent(i)) {
                        ImGui::Text("Slot %u: %s", i, AssetDB::idToPath(matSlots->getKeyForSlot(i)).c_str());
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

    VkResult nextImageRes = swapchain->acquireImage(device, imgAvailable[frameIdx], &imageIndex);

    if ((nextImageRes == VK_ERROR_OUT_OF_DATE_KHR || nextImageRes == VK_SUBOPTIMAL_KHR) && width != 0 && height != 0) {
        if (nextImageRes == VK_ERROR_OUT_OF_DATE_KHR)
            logVrb(WELogCategoryRender, "Swapchain out of date");
        else
            logVrb(WELogCategoryRender, "Swapchain suboptimal");
        recreateSwapchain();

        // acquire image from new swapchain
        swapchain->acquireImage(device, imgAvailable[frameIdx], &imageIndex);
    }

    if (imgFences[imageIndex] && imgFences[imageIndex] != cmdBufFences[frameIdx]) {
        VkResult result = vkWaitForFences(device, 1, &imgFences[imageIndex], true, UINT64_MAX);
        if (result != VK_SUCCESS) {
            std::string errStr = "Failed to wait on image fence: ";
            errStr += vku::toString(result);
            fatalErr(errStr.c_str());
        }
    }

    imgFences[imageIndex] = cmdBufFences[frameIdx];

    if (vkWaitForFences(device, 1, &cmdBufFences[frameIdx], VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        fatalErr("Failed to wait on fences");
    }

    if (vkResetFences(device, 1, &cmdBufFences[frameIdx]) != VK_SUCCESS) {
        fatalErr("Failed to reset fences");
    }

    VkCommandBuffer cmdBuf = cmdBufs[frameIdx];
    writeCmdBuf(cmdBuf, imageIndex, cam, reg);

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &imgAvailable[frameIdx];

    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.pWaitDstStageMask = &waitStages;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmdBuf;
    submit.pSignalSemaphores = &cmdBufferSemaphores[frameIdx];
    submit.signalSemaphoreCount = 1;

    if (enableVR)
        vr::VRCompositor()->SubmitExplicitTimingData();

    VkQueue queue = getQueue(device, graphicsQueueFamilyIdx);
    VkResult submitResult = vkQueueSubmit(queue, 1, &submit, cmdBufFences[frameIdx]);

    if (submitResult != VK_SUCCESS) {
        std::string errStr = vku::toString(submitResult);
        fatalErr(("Failed to submit queue (error: " + errStr + ")").c_str());
    }

    TracyMessageL("Queue submitted");

    if (enableVR) {
        submitToOpenVR();
    }

    VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    VkSwapchainKHR cSwapchain = swapchain->getSwapchain();
    presentInfo.pSwapchains = &cSwapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imageIndex;

    presentInfo.pWaitSemaphores = &cmdBufferSemaphores[frameIdx];
    presentInfo.waitSemaphoreCount = 1;

    VkResult presentResult = vkQueuePresentKHR(queue, &presentInfo);
    vr::VRCompositor()->PostPresentHandoff();

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
    } else if (presentResult == VK_SUBOPTIMAL_KHR) {
        logVrb(WELogCategoryRender, "swapchain after present suboptimal");
        //recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        fatalErr("Failed to present");
    }

    std::array<std::uint64_t, 2> timeStamps = { {0} };

    VkResult queryRes = vkGetQueryPoolResults(
        device,
        queryPool, 2 * lastFrameIdx, (uint32_t)timeStamps.size(),
        timeStamps.size() * sizeof(uint64_t), timeStamps.data(),
        sizeof(uint64_t), VK_QUERY_RESULT_64_BIT
    );

    if (queryRes == VK_SUCCESS)
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

    auto ext = AssetDB::getAssetExtension(id);
    auto path = AssetDB::idToPath(id);
    LoadedMeshData lmd;

    if (!PHYSFS_exists(path.c_str())) {
        logErr(WELogCategoryRender, "Mesh %s was missing!", path.c_str());
        loadWorldsModel(AssetDB::pathToId("Models/missing.wmdl"), vertices, indices, lmd);
    } else if (ext == ".obj") { // obj
        // Use C++ physfs ifstream for tinyobjloader
        PhysFS::ifstream meshFileStream(AssetDB::openAssetFileRead(id));
        loadObj(vertices, indices, meshFileStream, lmd);
        lmd.numSubmeshes = 1;
        lmd.submeshes[0].indexCount = indices.size();
        lmd.submeshes[0].indexOffset = 0;
    } else if (ext == ".mdl") { // source model
        std::filesystem::path mdlPath = AssetDB::idToPath(id);
        std::string vtxPath = mdlPath.parent_path().string() + "/" + mdlPath.stem().string();
        vtxPath += ".dx90.vtx";
        std::string vvdPath = mdlPath.parent_path().string() + "/" + mdlPath.stem().string();
        vvdPath += ".vvd";

        AssetID vtxId = AssetDB::pathToId(vtxPath);
        AssetID vvdId = AssetDB::pathToId(vvdPath);
        loadSourceModel(id, vtxId, vvdId, vertices, indices, lmd);
    } else if (ext == ".wmdl") {
        loadWorldsModel(id, vertices, indices, lmd);
    } else if (ext == ".rblx") {
        loadRobloxMesh(id, vertices, indices, lmd);
    }

    lmd.indexType = VK_INDEX_TYPE_UINT32;
    lmd.indexCount = (uint32_t)indices.size();
    lmd.ib = vku::IndexBuffer{ device, allocator, indices.size() * sizeof(uint32_t), "Mesh Index Buffer" };
    lmd.ib.upload(device, commandPool, getQueue(device, graphicsQueueFamilyIdx), indices);
    lmd.vb = vku::VertexBuffer{ device, allocator, vertices.size() * sizeof(Vertex), "Mesh Vertex Buffer" };
    lmd.vb.upload(device, commandPool, getQueue(device, graphicsQueueFamilyIdx), vertices);

    lmd.aabbMax = glm::vec3(0.0f);
    lmd.aabbMin = glm::vec3(std::numeric_limits<float>::max());
    lmd.sphereRadius = 0.0f;
    for (auto& vtx : vertices) {
        lmd.sphereRadius = std::max(glm::length(vtx.position), lmd.sphereRadius);
        lmd.aabbMax = glm::max(lmd.aabbMax, vtx.position);
        lmd.aabbMin = glm::min(lmd.aabbMin, vtx.position);
    }

    logVrb(WELogCategoryRender, "Loaded mesh %u, %u verts. Sphere radius %f", id, (uint32_t)vertices.size(), lmd.sphereRadius);

    loadedMeshes.insert({ id, std::move(lmd) });
}

void VKRenderer::unloadUnusedMaterials(entt::registry& reg) {
    ZoneScoped;

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
    VKCHECK(vkDeviceWaitIdle(device));
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
        for (uint32_t i = 0; i < NUM_CUBEMAP_SLOTS; i++) {
            if (cubemapSlots->isSlotPresent(i))
                cubemapSlots->unload(i);
        }
    }

    clearMaterialIndices = true;
    if (enumHasFlag(flags, ReloadFlags::Meshes)) {
        loadedMeshes.clear();
    }
}

RenderResources VKRenderer::getResources() {
    return RenderResources{
        *texSlots,
        *cubemapSlots,
        *matSlots,
        loadedMeshes,
        &brdfLut,
        &materialUB,
        &vpBuffer,
        shadowmapImage,
        shadowImages
    };
}

VKRTTPass* VKRenderer::createRTTPass(RTTPassCreateInfo& ci) {
    VKRTTPass* pass = new VKRTTPass{
        ci,
        this,
        vrInterface,
        frameIdx,
        &dbgStats
    };

    rttPasses.push_back(pass);
    return pass;
}

void VKRenderer::destroyRTTPass(RTTPass* pass) {
    rttPasses.erase(
        std::remove(rttPasses.begin(), rttPasses.end(), pass),
        rttPasses.end());

    delete pass;
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
        VKCHECK(vkDeviceWaitIdle(device));
        auto physDevProps = getPhysicalDeviceProperties(physicalDevice);
        PipelineCacheSerializer::savePipelineCache(physDevProps, pipelineCache, device);

        ShaderCache::clear();

        for (auto& semaphore : cmdBufferSemaphores) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }

        for (auto& semaphore : imgAvailable) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }

        for (auto& fence : cmdBufFences) {
            vkDestroyFence(device, fence, nullptr);
        }

        std::vector<VKRTTPass*> toDelete;
        for (auto& p : rttPasses) {
            toDelete.push_back(p);
        }

        for (auto& p : toDelete) {
            destroyRTTPass(p);
        }

        rttPasses.clear();
        delete irp;

        texSlots.reset();
        matSlots.reset();
        cubemapSlots.reset();
        cubemapConvoluter.reset();

        brdfLut.destroy();
        loadedMeshes.clear();

        delete shadowCascadePass;
        delete additionalShadowsPass;

        delete imguiImage;
        delete shadowmapImage;
        delete finalPrePresent;

        if (leftEye) {
            delete leftEye;
            delete rightEye;
        }

        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
            delete shadowImages[i];
        }

        materialUB.destroy();
        vpBuffer.destroy();


#ifndef NDEBUG
        char* statsString;
        vmaBuildStatsString(allocator, &statsString, true);

        FILE* file = fopen("memory_shutdown.json", "w");
        fwrite(statsString, strlen(statsString), 1, file);
        fclose(file);

        vmaFreeStatsString(allocator, statsString);
#endif
        vmaDestroyAllocator(allocator);

        delete swapchain;

        vkDestroySurfaceKHR(instance, surface, nullptr);
        logVrb(WELogCategoryRender, "Renderer destroyed.");

        vkDestroyQueryPool(device, queryPool, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        vkDestroyDevice(device, nullptr);
        dbgCallback.reset();
        vkDestroyInstance(instance, nullptr);
    }
}
