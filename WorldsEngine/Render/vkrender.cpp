#include <Util/TimingUtil.hpp>
#define _CRT_SECURE_NO_WARNINGS
#include <Libs/volk.h>
#include "vku/vku.hpp"
#include <Core/Engine.hpp>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <ImGui/imgui_impl_vulkan.h>
#include <IO/physfs.hpp>
#include <Core/Transform.hpp>
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
#include <tracy/TracyVulkan.hpp>
#include "RenderPasses.hpp"
#include <Input/Input.hpp>
#include <VR/OpenVRInterface.hpp>
#include <Core/Fatal.hpp>
#include <unordered_set>
#include <Core/Log.hpp>
#include "Loaders/ObjModelLoader.hpp"
#include "Render.hpp"
#include "Loaders/SourceModelLoader.hpp"
#include "Loaders/WMDLLoader.hpp"
#include "Loaders/RobloxMeshLoader.hpp"
#include "ShaderCache.hpp"
#include "RenderInternal.hpp"
#define RDOC
#ifdef RDOC
#include "renderdoc_app.h"
#include <slib/DynamicLibrary.hpp>
#endif
#include <Util/EnumUtil.hpp>
#include "vku/DeviceMaker.hpp"
#include "vku/InstanceMaker.hpp"
#include <Core/JobSystem.hpp>
#include <Core/Console.hpp>

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

RenderResource* VKRenderer::createTextureResource(TextureResourceCreateInfo resourceCreateInfo, const char* debugName) {
    RenderResource* resource = new RenderResource;

    VkImageCreateInfo ici { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };

    switch (resourceCreateInfo.type) {
    case TextureType::T1D:
        ici.imageType = VK_IMAGE_TYPE_1D;
    case TextureType::T2D:
    case TextureType::T2DArray:
        ici.imageType = VK_IMAGE_TYPE_2D;
        break;
    case TextureType::T3D:
        ici.imageType = VK_IMAGE_TYPE_3D;
        break;
    case TextureType::TCube:
        ici.imageType = VK_IMAGE_TYPE_2D;
        break;
    }

    ici.extent.depth  = resourceCreateInfo.depth;
    ici.extent.width  = resourceCreateInfo.width;
    ici.extent.height = resourceCreateInfo.height;
    ici.mipLevels = resourceCreateInfo.mipLevels;
    ici.arrayLayers = resourceCreateInfo.layers;
    ici.initialLayout = resourceCreateInfo.initialLayout;
    ici.format = resourceCreateInfo.format;

    if (resourceCreateInfo.type == TextureType::TCube) {
        ici.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        ici.arrayLayers = 6;
    }

    ici.samples = vku::sampleCountFlags(resourceCreateInfo.samples);
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = resourceCreateInfo.usage;

    switch (resourceCreateInfo.sharingMode) {
    case SharingMode::Concurrent:
        ici.sharingMode = VK_SHARING_MODE_CONCURRENT;
        break;
    case SharingMode::Exclusive:
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        break;
    }

    VkImageViewType viewType;

    switch (resourceCreateInfo.type) {
    case TextureType::T1D:
        viewType = VK_IMAGE_VIEW_TYPE_1D;
        break;
    case TextureType::T2D:
        viewType = VK_IMAGE_VIEW_TYPE_2D;
        break;
    case TextureType::T2DArray:
        viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        break;
    case TextureType::T3D:
        viewType = VK_IMAGE_VIEW_TYPE_3D;
        break;
    case TextureType::TCube:
        viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        break;
    }

    resource->name = debugName;
    resource->type = ResourceType::Image;
    resource->resource = std::make_unique<vku::GenericImage>(
        device, allocator, ici, viewType,
        resourceCreateInfo.aspectFlags, false, debugName);

    return resource;
}

void VKRenderer::updateTextureResource(RenderResource* resource, TextureResourceCreateInfo resourceCreateInfo) {
    assert(resource->type == ResourceType::Image);
    VkImageLayout oldLayout = resource->image().layout();
    VkImageCreateInfo ici { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };

    switch (resourceCreateInfo.type) {
    case TextureType::T1D:
        ici.imageType = VK_IMAGE_TYPE_1D;
    case TextureType::T2D:
    case TextureType::T2DArray:
        ici.imageType = VK_IMAGE_TYPE_2D;
        break;
    case TextureType::T3D:
        ici.imageType = VK_IMAGE_TYPE_3D;
        break;
    case TextureType::TCube:
        ici.imageType = VK_IMAGE_TYPE_2D;
        break;
    }

    ici.extent.depth  = resourceCreateInfo.depth;
    ici.extent.width  = resourceCreateInfo.width;
    ici.extent.height = resourceCreateInfo.height;
    ici.mipLevels = resourceCreateInfo.mipLevels;
    ici.arrayLayers = resourceCreateInfo.layers;
    ici.initialLayout = resourceCreateInfo.initialLayout;
    ici.format = resourceCreateInfo.format;

    if (resourceCreateInfo.type == TextureType::TCube) {
        ici.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        ici.arrayLayers = 6;
    }

    ici.samples = vku::sampleCountFlags(resourceCreateInfo.samples);
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = resourceCreateInfo.usage;

    switch (resourceCreateInfo.sharingMode) {
    case SharingMode::Concurrent:
        ici.sharingMode = VK_SHARING_MODE_CONCURRENT;
        break;
    case SharingMode::Exclusive:
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        break;
    }

    VkImageViewType viewType;

    switch (resourceCreateInfo.type) {
    case TextureType::T1D:
        viewType = VK_IMAGE_VIEW_TYPE_1D;
        break;
    case TextureType::T2D:
        viewType = VK_IMAGE_VIEW_TYPE_2D;
        break;
    case TextureType::T2DArray:
        viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        break;
    case TextureType::T3D:
        viewType = VK_IMAGE_VIEW_TYPE_3D;
        break;
    case TextureType::TCube:
        viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        break;
    }

    resource->resource.reset();
    resource->resource = std::make_unique<vku::GenericImage>(
        device, allocator, ici, viewType,
        resourceCreateInfo.aspectFlags, false, resource->name.c_str());

    vku::executeImmediately(handles.device, handles.commandPool, queues.graphics, [&](VkCommandBuffer cmdbuf) {
        resource->image().setLayout(cmdbuf, oldLayout, resourceCreateInfo.aspectFlags);
    });
}

void VKRenderer::createFramebuffers() {
    Swapchain& swapchain = presentSubmitManager->currentSwapchain();
    framebuffers.resize(swapchain.images.size());
    for (size_t i = 0; i != swapchain.images.size(); i++) {
        VkImageView attachments[1] = { swapchain.imageViews[i] };
        VkFramebufferCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.attachmentCount = 1;
        fci.pAttachments = attachments;
        fci.width = width;
        fci.height = height;
        fci.renderPass = irp->getRenderPass();
        fci.layers = 1;

        VKCHECK(vku::createFramebuffer(device, &fci, &framebuffers[i]));
    }
}

bool shouldEnableValidation(bool enableVR) {
    bool activateValidationLayers = EngineArguments::hasArgument("validation-layers");
#ifndef NDEBUG
    activateValidationLayers |= !enableVR;
#endif
    return activateValidationLayers;
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

    if (shouldEnableValidation(enableVR)) {
        logVrb(WELogCategoryRender, "Activating validation layers");
        instanceMaker.layer("VK_LAYER_KHRONOS_validation");
        instanceMaker.extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

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
        logVrb(worlds::WELogCategoryRender, "Memory type %i: heap %i, %s", i, memType.heapIndex, str);
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

VkBool32 canPresentFromCompute = false;

VKRenderer::VKRenderer(const RendererInitInfo& initInfo, bool* success)
    : finalPrePresent(nullptr)
    , leftEye(nullptr)
    , rightEye(nullptr)
    , shadowmapImage(nullptr)
    , imguiImage(nullptr)
    , window(initInfo.window)
    , shadowmapRes(2048)
    , enableVR(initInfo.enableVR)
    , irp(nullptr)
    , vrPredictAmount(0.033f)
    , useVsync(true)
    , frameIdx(0)
    , lastFrameIdx(0) {
    msaaSamples = VK_SAMPLE_COUNT_2_BIT;
    numMSAASamples = 2;

#ifdef RDOC
#ifdef _WIN32
    const char* renderdocModule = "renderdoc.dll";
#else
    const char* renderdocModule = "librenderdoc.so";
#endif
    slib::DynamicLibrary dl{renderdocModule};
    if (dl.loaded()) {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)dl.getFunctionPointer("RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdocApi);

        if (ret != 1) {
            logWarn(WELogCategoryRender, "Failed to get RenderDoc API");
            rdocApi = nullptr;
        } else {
            RENDERDOC_API_1_1_2* api = (RENDERDOC_API_1_1_2*)rdocApi;
            api->SetCaptureFilePathTemplate("rdocCaptures");
            g_console->registerCommand([api](void*, const char*) {
                api->TriggerCapture();
            }, "r_triggerRenderdocCapture", "Triggers a Renderdoc capture.", nullptr);
        }
    } else {
        logWarn(WELogCategoryRender, "Failed to load RenderDoc dynamic library");
        rdocApi = nullptr;
    }
#endif

    apiMutex = new std::mutex;

    // Initialize and create instance
    if (volkInitialize() != VK_SUCCESS) {
        fatalErr("Couldn't find Vulkan.");
    }

    createInstance(initInfo);
    volkLoadInstance(instance);

    if (shouldEnableValidation(enableVR))
        dbgCallback = vku::DebugCallback(instance);

    // Select an appropriate physical device
    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

    std::vector<VkPhysicalDevice> physDevs;
    physDevs.resize(physicalDeviceCount);
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physDevs.data());

    physicalDevice = pickPhysicalDevice(physDevs);

    logPhysDevInfo(physicalDevice);

    // Now find the queues
    std::vector<VkQueueFamilyProperties> qprops = getQueueFamilies(physicalDevice);

    const auto badQueue = ~(uint32_t)0;
    queues.graphicsIdx = badQueue;
    VkQueueFlags search = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

    // Look for a queue family with both graphics and
    // compute first.
    for (uint32_t qi = 0; qi != qprops.size(); ++qi) {
        auto& qprop = qprops[qi];
        if ((qprop.queueFlags & search) == search) {
            queues.graphicsIdx = qi;
            break;
        }
    }

    // Search for async compute queue family
    queues.asyncComputeIdx = badQueue;
    for (size_t i = 0; i < qprops.size(); i++) {
        auto& qprop = qprops[i];
        if ((qprop.queueFlags & (VK_QUEUE_COMPUTE_BIT)) == VK_QUEUE_COMPUTE_BIT && i != queues.graphicsIdx) {
            queues.asyncComputeIdx = i;
            break;
        }
    }

    if (queues.asyncComputeIdx == badQueue)
        logWarn(worlds::WELogCategoryRender, "Couldn't find async compute queue");

    if (queues.graphicsIdx == badQueue) {
        *success = false;
        return;
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
    case 0x5143:
        vendor = VKVendor::Qualcomm;
        break;
    }

    uint32_t count;
    VKCHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> extensionProperties;
    VKCHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr));

    vku::DeviceMaker dm{};
    dm.defaultLayers();
    dm.queue(queues.graphicsIdx);

    if (queues.asyncComputeIdx != badQueue)
        dm.queue(queues.asyncComputeIdx);

    // Create surface and find presentation queue
    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, instance, &surface);

    this->surface = surface;
    queues.presentIdx = findPresentQueue(physicalDevice, surface);

    if (queues.asyncComputeIdx != badQueue)
        VKCHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queues.asyncComputeIdx, surface, &canPresentFromCompute));

    if (queues.presentIdx != queues.graphicsIdx)
        dm.queue(queues.presentIdx);

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

    bool hasRasterizationOrderExtension = std::find_if(extensionProperties.begin(), extensionProperties.end(),
            [](VkExtensionProperties props) {return strcmp(props.extensionName, VK_AMD_RASTERIZATION_ORDER_EXTENSION_NAME) == 0; }) != extensionProperties.end();

    if (hasRasterizationOrderExtension)
        dm.extension(VK_AMD_RASTERIZATION_ORDER_EXTENSION_NAME);

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

    queues.graphics = getQueue(device, queues.graphicsIdx);
    queues.present = getQueue(device, queues.presentIdx);

    if (queues.asyncComputeIdx != badQueue)
        queues.asyncCompute = getQueue(device, queues.asyncComputeIdx);

    VmaAllocatorCreateInfo allocatorCreateInfo{};
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.frameInUseCount = 0;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorCreateInfo.flags = 0;
    vmaCreateAllocator(&allocatorCreateInfo, &allocator);

    // Initialize the deletion queue
    DeletionQueue::intitialize(device, allocator);
    DeletionQueue::resize(1);

    VkPipelineCacheCreateInfo pipelineCacheInfo{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    PipelineCacheSerializer::loadPipelineCache(getPhysicalDeviceProperties(physicalDevice), pipelineCacheInfo);

    VKCHECK(vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr, &pipelineCache));
    std::free((void*)pipelineCacheInfo.pInitialData);

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.emplace_back(VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 });
    poolSizes.emplace_back(VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 });
    poolSizes.emplace_back(VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 });

    // Create an arbitrary number of descriptors in a pool.
    // Allow the descriptors to be freed, possibly not optimal behaviour.
    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolInfo.maxSets = 256;
    descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    descriptorPoolInfo.pPoolSizes = poolSizes.data();

    VKCHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

    // Command pool
    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = queues.graphicsIdx;

    VKCHECK(vkCreateCommandPool(device, &cpci, nullptr, &commandPool));
    vku::setObjectName(device, (uint64_t)commandPool, VK_OBJECT_TYPE_COMMAND_POOL, "Main Command Pool");

    if (initInfo.activeVrApi == VrApi::OpenVR) {
        OpenVRInterface* vrInterface = static_cast<OpenVRInterface*>(initInfo.vrInterface);
        vrInterface->getRenderResolution(&vrWidth, &vrHeight);
    }

    handles = VulkanHandles{
        vendor,
        hasRasterizationOrderExtension,
        physicalDevice,
        device,
        pipelineCache,
        descriptorPool,
        commandPool,
        instance,
        allocator,
        queues.graphicsIdx,
        GraphicsSettings {
            numMSAASamples,
            (int)shadowmapRes,
            enableVR
        },
        width, height,
        vrWidth, vrHeight
    };

    auto vkCtx = std::make_shared<VulkanHandles>(handles);

    presentSubmitManager = std::make_unique<VKPresentSubmitManager>(window, surface, &handles, &queues, &dbgStats);
    presentSubmitManager->recreateSwapchain(useVsync && !enableVR, width, height);
#ifdef TRACY_ENABLE
    presentSubmitManager->setupTracyContexts(tracyContexts);
#endif

    VKCHECK(vkDeviceWaitIdle(device));
    DeletionQueue::cleanupFrame(0);
    DeletionQueue::resize(presentSubmitManager->numFramesInFlight());
    DeletionQueue::setCurrentFrame(presentSubmitManager->numFramesInFlight() - 1);

    cubemapConvoluter = std::make_shared<CubemapConvoluter>(vkCtx);
    uiTextureMan = new VKUITextureManager(handles);

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
        VK_SHARING_MODE_EXCLUSIVE, queues.graphicsIdx
    };

    brdfLut = vku::GenericImage{ device, allocator, brdfLutIci, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, false, "BRDF LUT" };

    vku::executeImmediately(device, commandPool, queues.graphics, [&](auto cb) {
        brdfLut.setLayout(cb, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        });

    BRDFLUTRenderer brdfLutRenderer{ *vkCtx };
    brdfLutRenderer.render(*vkCtx, brdfLut);

    createCascadeShadowImages();
    createSpotShadowImages();

    createSCDependents();

    timestampPeriod = physDevProps.limits.timestampPeriod;

    VkQueryPoolCreateInfo qpci{};
    qpci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qpci.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qpci.queryCount = 2 * presentSubmitManager->numFramesInFlight();

    VKCHECK(vkCreateQueryPool(device, &qpci, nullptr, &queryPool));

    *success = true;

    if (enableVR) {
        if (initInfo.activeVrApi == VrApi::OpenVR) {
            vr::VRCompositor()->SetExplicitTimingMode(vr::EVRCompositorTimingMode::VRCompositorTimingMode_Explicit_ApplicationPerformsPostPresentHandoff);
        }

        vrInterface = initInfo.vrInterface;
        vrApi = initInfo.activeVrApi;
    } else {
        vrApi = VrApi::None;
    }

    // Load cubemap for the sky
    cubemapSlots->loadOrGet(AssetDB::pathToId("envmap_miramar/miramar.json"));

    g_console->registerCommand([&](void*, const char* arg) {
        std::lock_guard<std::mutex> lg {*apiMutex};
        numMSAASamples = std::atoi(arg);
        // The sample count flags are actually identical to the number of samples
        msaaSamples = (VkSampleCountFlagBits)numMSAASamples;
        handles.graphicsSettings.msaaLevel = numMSAASamples;
        }, "r_setMSAASamples", "Sets the number of MSAA samples. Does not automatically recreate RTT passes!", nullptr);

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
        std::lock_guard<std::mutex> lg {*apiMutex};
        shadowmapRes = std::atoi(arg);
        handles.graphicsSettings.shadowmapRes = shadowmapRes;
        delete shadowmapImage;

        createCascadeShadowImages();

        delete shadowCascadePass;
        shadowCascadePass = new ShadowCascadePass(vrInterface, &handles, shadowmapImage);
        shadowCascadePass->setup();

        for (VKRTTPass* rttPass : rttPasses) {
            rttPass->prp->reuploadDescriptors();
        }
        }, "r_setCSMResolution", "Sets the resolution of the cascaded shadow map.");

    g_console->registerCommand([&](void*, const char* arg) {
        std::lock_guard<std::mutex> lg {*apiMutex};
        handles.graphicsSettings.spotShadowmapRes = std::atoi(arg);

        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
            delete shadowImages[i];
        }
        createSpotShadowImages();

        delete additionalShadowsPass;
        additionalShadowsPass = new AdditionalShadowsPass(&handles);
        additionalShadowsPass->setup(getResources());
        for (VKRTTPass* rttPass : rttPasses) {
            rttPass->prp->reuploadDescriptors();
        }

        }, "r_setSpotShadowResolution", "Sets the resolution of spotlight shadows.");

    materialUB = vku::GenericBuffer(
        device, allocator,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof(MaterialsUB), VMA_MEMORY_USAGE_GPU_ONLY, "Materials");

    vpBuffer = vku::GenericBuffer(
        device, allocator,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof(MultiVP), VMA_MEMORY_USAGE_GPU_ONLY, "VP Buffer");

    MaterialsUB materials{};
    materialUB.upload(device, commandPool, queues.graphics, &materials, sizeof(materials));

    shadowCascadePass = new ShadowCascadePass(vrInterface, &handles, shadowmapImage);
    shadowCascadePass->setup();

    additionalShadowsPass = new AdditionalShadowsPass(&handles);
    additionalShadowsPass->setup(getResources());
    VKCHECK(vkDeviceWaitIdle(device));
    DeletionQueue::cleanupFrame(0);
}

void VKRenderer::createSpotShadowImages() {
    for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
        int spotRes = handles.graphicsSettings.spotShadowmapRes;
        TextureResourceCreateInfo shadowCreateInfo{
            TextureType::T2D,
            VK_FORMAT_D32_SFLOAT,
            spotRes, spotRes,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        };

        shadowImages[i] = createTextureResource(shadowCreateInfo, ("Shadow Image " + std::to_string(i)).c_str());
    }

    vku::executeImmediately(device, commandPool, queues.graphics, [&](auto cb) {
        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
            shadowImages[i]->image().setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        }
        });
}

void VKRenderer::createCascadeShadowImages() {
    TextureResourceCreateInfo shadowmapCreateInfo{
        TextureType::T2DArray,
        VK_FORMAT_D32_SFLOAT,
        (int)shadowmapRes, (int)shadowmapRes,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    };
    shadowmapCreateInfo.layers = 3;
    shadowmapImage = createTextureResource(shadowmapCreateInfo, "Shadowmap Image");

    vku::executeImmediately(device, commandPool, queues.graphics, [&](auto cb) {
        shadowmapImage->image().setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        });
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
        irp = new ImGuiRenderPass(&handles, presentSubmitManager->currentSwapchain());
        irp->setup();
    }

    createFramebuffers();

    TextureResourceCreateInfo imguiImageCreateInfo{
        TextureType::T2D,
        VK_FORMAT_R8G8B8A8_UNORM,
        (int)width, (int)height,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
    };

    imguiImage = createTextureResource(imguiImageCreateInfo, "ImGui Image");

    imguiImageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    finalPrePresent = createTextureResource(imguiImageCreateInfo, "Final Pre-Present");

    if (enableVR) {
        imguiImageCreateInfo.width = vrWidth;
        imguiImageCreateInfo.height = vrHeight;
        leftEye = createTextureResource(imguiImageCreateInfo, "Left Eye");
        rightEye = createTextureResource(imguiImageCreateInfo, "Right Eye");
    }

    vku::executeImmediately(device, commandPool, queues.graphics, [&](VkCommandBuffer cmdBuf) {
        finalPrePresent->image().setLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        });

    if (vrApi == VrApi::OpenVR) {
        OpenVRInterface* vrInterface = static_cast<OpenVRInterface*>(vrInterface);
        vrInterface->getRenderResolution(&vrWidth, &vrHeight);
    }

    for (auto& p : rttPasses) {
        if (p->outputToScreen) {
            // Recreate pass
            p->destroy();
            p->width = p->isVr ? vrWidth : width;
            p->height = p->isVr ? vrHeight : height;
            p->createInfo.width = p->width;
            p->createInfo.height = p->height;
            p->create(this, vrInterface, frameIdx, &dbgStats);
            p->setFinalPrePresents();
        }
    }
}

VkSurfaceCapabilitiesKHR getSurfaceCaps(VkPhysicalDevice pd, VkSurfaceKHR surf) {
    VkSurfaceCapabilitiesKHR surfCaps;
    VKCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surf, &surfCaps));
    return surfCaps;
}

void VKRenderer::recreateSwapchain(int newWidth, int newHeight) {
    std::lock_guard<std::mutex> lg { *apiMutex };
    recreateSwapchainInternal(newWidth, newHeight);
}

void VKRenderer::recreateSwapchainInternal(int newWidth, int newHeight) {
    // Wait for current frame to finish
    vkDeviceWaitIdle(device);

    // Check width/height - if it's 0, just ignore it
    auto surfaceCaps = getSurfaceCaps(physicalDevice, surface);

    if (newWidth < 0 || newHeight < 0) {
        newWidth = surfaceCaps.currentExtent.width;
        newHeight = surfaceCaps.currentExtent.height;
    }

    if (newWidth == 0 || newHeight == 0) {
        logVrb(WELogCategoryRender, "Ignoring resize with 0 width or height");
        isMinimised = true;
        return;
    }

    isMinimised = false;

    logVrb(WELogCategoryRender, "Recreating swapchain: New surface size is %ix%i",
        newWidth, newHeight);

    width = newWidth;
    height = newHeight;

    framebuffers.clear();

    presentSubmitManager->recreateSwapchain(useVsync && !enableVR, width, height);
    createSCDependents();

    swapchainRecreated = true;
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

    VkImage vkImg = leftEye->image().image();

    vr::VRVulkanTextureData_t vulkanData{
        .m_nImage = (uint64_t)vkImg,
        .m_pDevice = device,
        .m_pPhysicalDevice = (VkPhysicalDevice_T*)physicalDevice,
        .m_pInstance = instance,
        .m_pQueue = queues.graphics,
        .m_nQueueFamilyIndex = queues.graphicsIdx,
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

        vulkanData.m_nImage = (uint64_t)(VkImage)rightEye->image().image();
        vr::VRCompositor()->Submit(vr::Eye_Right, &texture, &bounds);
    }
}

void VKRenderer::uploadSceneAssetsForReg(entt::registry& reg, bool& reuploadMats, bool& dsUpdateNeeded) {
    std::unordered_set<AssetID> uploadMats;
    std::unordered_set<AssetID> uploadMeshes;

    // Upload any necessary materials + meshes
    reg.view<WorldObject>().each([&](WorldObject& wo) {
        for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
            if (!wo.presentMaterials[i]) continue;

            if (!matSlots->isLoaded(wo.materials[i])) {
                reuploadMats = true;
                uploadMats.insert(wo.materials[i]);
            }
        }

        if (loadedMeshes.find(wo.mesh) == loadedMeshes.end()) {
            uploadMeshes.insert(wo.mesh);
        }
        });

    reg.view<SkinnedWorldObject>().each([&](SkinnedWorldObject& wo) {
        for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
            if (!wo.presentMaterials[i]) continue;

            if (!matSlots->isLoaded(wo.materials[i])) {
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

            if (!matSlots->isLoaded(wo.materials[i])) {
                matSlots->loadOrGet(wo.materials[i]);
                reuploadMats = true;
            }
        }
        });

    for (AssetID id : uploadMeshes) {
        preloadMesh(id);
    }

    AssetID skyboxId = reg.ctx<SceneSettings>().skybox;

    if (!cubemapSlots->isLoaded(skyboxId)) {
        cubemapSlots->loadOrGet(skyboxId);
    }

    reg.view<WorldCubemap>().each([&](WorldCubemap& wc) {
        if (!cubemapSlots->isLoaded(wc.cubemapId)) {
            cubemapSlots->loadOrGet(wc.cubemapId);
            dsUpdateNeeded = true;
        }
        });
}

void VKRenderer::uploadSceneAssets(entt::registry& reg) {
    ZoneScoped;
    bool reuploadMats = false;
    bool dsUpdateNeeded = false;

    uploadSceneAssetsForReg(reg, reuploadMats, dsUpdateNeeded);

    for (auto& pass : rttPasses) {
        if (pass->createInfo.registryOverride) {
            uploadSceneAssetsForReg(*pass->createInfo.registryOverride, reuploadMats, dsUpdateNeeded);
        }
    }

    dsUpdateNeeded |= reuploadMats;

    if (reuploadMats)
        reuploadMaterials();
    else if (dsUpdateNeeded) {
        for (auto& pass : rttPasses) {
            pass->prp->reuploadDescriptors();
        }
        additionalShadowsPass->reuploadDescriptors();
    }
}

void VKRenderer::writeCmdBuf(VkCommandBuffer cmdBuf, uint32_t imageIndex, Camera& cam, entt::registry& reg) {
    ZoneScoped;
    PerfTimer pt;

    RenderContext rCtx{
        .resources = getResources(),
        .cascadeInfo = {},
        .debugContext = RenderDebugContext {
            .stats = &dbgStats
#ifdef TRACY_ENABLE
            , .tracyContexts = &tracyContexts
#endif
        },
        .passSettings = this->handles.graphicsSettings,
        .registry = reg,
        .renderer = this,
        .cmdBuf = cmdBuf,
        .frameIndex = frameIdx,
        .maxSimultaneousFrames = presentSubmitManager->numFramesInFlight()
    };

    uploadSceneAssets(reg);

    std::sort(rttPasses.begin(), rttPasses.end(), [](VKRTTPass* a, VKRTTPass* b) {
        return a->drawSortKey < b->drawSortKey;
        });

    additionalShadowsPass->prePass(rCtx);

    for (auto& p : rttPasses) {
        if (!p->active) continue;

        bool nullCam = p->cam == nullptr;

        if (nullCam)
            p->cam = &cam;

        p->prePass(frameIdx, reg);

        if (nullCam)
            p->cam = nullptr;
    }

    vku::beginCommandBuffer(cmdBuf, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    texSlots->frameStarted = true;

    vkCmdResetQueryPool(cmdBuf, queryPool, 2 * frameIdx, 2);
    vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, frameIdx * 2);

    texSlots->setUploadCommandBuffer(cmdBuf, frameIdx);

    finalPrePresent->image().setLayout(cmdBuf,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    additionalShadowsPass->execute(rCtx);

    int numActivePasses = 0;
    bool lastPassIsVr = false;
    for (auto& p : rttPasses) {
        if (!p->active) continue;
        numActivePasses++;

        if (!p->outputToScreen) {
            p->sdrFinalTarget->image().setLayout(cmdBuf,
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
            p->sdrFinalTarget->image().setLayout(cmdBuf,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT);
        }
        lastPassIsVr = p->isVr;
    }
    dbgStats.numActiveRTTPasses = numActivePasses;
    dbgStats.numRTTPasses = rttPasses.size();

    vku::transitionLayout(cmdBuf, presentSubmitManager->currentSwapchain().images[imageIndex],
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);


    if (enableVR) {
        leftEye->image().setLayout(cmdBuf,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT);

        rightEye->image().setLayout(cmdBuf,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT);
    }

    finalPrePresent->image().setLayout(cmdBuf,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT);

    if (!lastPassIsVr) {
        VkImageBlit imageBlit{};
        imageBlit.srcOffsets[1] = imageBlit.dstOffsets[1] = VkOffset3D{ (int)width, (int)height, 1 };
        imageBlit.dstSubresource = imageBlit.srcSubresource = VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        vkCmdBlitImage(cmdBuf, finalPrePresent->image().image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            presentSubmitManager->currentSwapchain().images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_NEAREST);
    } else {
        // Calculate the best crop for the current window size against the VR render target
        float aspect = (float)windowSize.y / (float)windowSize.x;
        float croppedHeight = aspect * vrWidth;

        glm::vec2 srcCorner0(0.0f, vrHeight / 2.0f - croppedHeight / 2.0f);
        glm::vec2 srcCorner1(vrWidth, vrHeight / 2.0f + croppedHeight / 2.0f);

        VkImageBlit imageBlit{};
        imageBlit.srcOffsets[0] = VkOffset3D{ (int)srcCorner0.x, (int)srcCorner0.y, 0 };
        imageBlit.srcOffsets[1] = VkOffset3D{ (int)srcCorner1.x, (int)srcCorner1.y, 1 };
        imageBlit.dstOffsets[1] = VkOffset3D{ (int)windowSize.x, (int)windowSize.y, 1 };
        imageBlit.dstSubresource = imageBlit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

        vkCmdBlitImage(cmdBuf, leftEye->image().image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            presentSubmitManager->currentSwapchain().images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
    }

    vku::transitionLayout(cmdBuf, presentSubmitManager->currentSwapchain().images[imageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);

    irp->execute(cmdBuf, width, height, framebuffers[imageIndex], imDrawData);

    ::imageBarrier(cmdBuf, presentSubmitManager->currentSwapchain().images[imageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_MEMORY_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1 + (frameIdx * 2));
#ifdef TRACY_ENABLE
    TracyVkCollect(tracyContexts[imageIndex], cmdBuf);
#endif
    texSlots->frameStarted = false;
    VKCHECK(vkEndCommandBuffer(cmdBuf));
    dbgStats.cmdBufWriteTime = pt.stopGetMs();
}

void VKRenderer::reuploadMaterials() {
    ZoneScoped;

    for (auto& pass : rttPasses) {
        pass->prp->reuploadDescriptors();
    }
    additionalShadowsPass->reuploadDescriptors();

    materialUB.upload(device, commandPool, queues.graphics, matSlots->getSlots(), sizeof(PackedMaterial) * NUM_MAT_SLOTS);
}

ConVar showSlotDebug{ "r_debugSlots", "0", "Shows a window for debugging resource slots." };

void VKRenderer::frame(Camera& cam, entt::registry& reg) {
    ZoneScoped;
    std::unique_lock<std::mutex> lock{*apiMutex};


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

    dbgStats.numCulledObjs = 0;
    dbgStats.numDrawCalls = 0;
    dbgStats.numPipelineSwitches = 0;
    dbgStats.numTriangles = 0;

    VkCommandBuffer cmdBuf;
    int imageIndex;
    frameIdx = presentSubmitManager->acquireFrame(cmdBuf, imageIndex);
    DeletionQueue::cleanupFrame(frameIdx);
    DeletionQueue::setCurrentFrame(frameIdx);

    if (!isMinimised || enableVR)
        writeCmdBuf(cmdBuf, imageIndex, cam, reg);
    else {
        vku::beginCommandBuffer(cmdBuf, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VKCHECK(vkEndCommandBuffer(cmdBuf));
    }

    if (enableVR) {
        vr::VRCompositor()->SubmitExplicitTimingData();
        presentSubmitManager->submit();
        submitToOpenVR();
        presentSubmitManager->present();
    } else {
        presentSubmitManager->submit();
        presentSubmitManager->present();
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

    lastFrameIdx = frameIdx;

    frameIdx++;
    frameIdx %= presentSubmitManager->numFramesInFlight();
    FrameMark;
}

void VKRenderer::preloadMesh(AssetID id) {
    ZoneScoped;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<VertSkinningInfo> vertSkinInfos;

    auto ext = AssetDB::getAssetExtension(id);
    auto path = AssetDB::idToPath(id);
    LoadedMeshData lmd;

    if (!PHYSFS_exists(path.c_str())) {
        logErr(WELogCategoryRender, "Mesh %s was missing!", path.c_str());
        loadWorldsModel(AssetDB::pathToId("Models/missing.wmdl"), vertices, indices, vertSkinInfos, lmd);
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
        loadWorldsModel(id, vertices, indices, vertSkinInfos, lmd);
    } else if (ext == ".rblx") {
        loadRobloxMesh(id, vertices, indices, lmd);
    }

    lmd.indexType = VK_INDEX_TYPE_UINT32;
    lmd.indexCount = (uint32_t)indices.size();
    lmd.ib = vku::IndexBuffer{ device, allocator, indices.size() * sizeof(uint32_t), "Mesh Index Buffer" };
    lmd.ib.upload(device, commandPool, queues.graphics, indices);
    lmd.vb = vku::VertexBuffer{ device, allocator, vertices.size() * sizeof(Vertex), "Mesh Vertex Buffer" };
    lmd.vb.upload(device, commandPool, queues.graphics, vertices);

    if (lmd.isSkinned) {
        lmd.vertexSkinWeights = vku::VertexBuffer{ device, allocator, vertSkinInfos.size() * sizeof(VertSkinningInfo), "Mesh Skinning Info" };
        lmd.vertexSkinWeights.upload(device, commandPool, queues.graphics, vertSkinInfos);
    }

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

void VKRenderer::unloadUnusedAssets(entt::registry& reg) {
    ZoneScoped;

    bool textureReferenced[NUM_TEX_SLOTS] { 0 };
    bool materialReferenced[NUM_MAT_SLOTS] { 0 };

    reg.view<WorldObject>().each([&materialReferenced, &textureReferenced, this](entt::entity, WorldObject& wo) {
        for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
            if (!wo.presentMaterials[i]) continue;
            uint32_t materialIndex = matSlots->get(wo.materials[i]);
            materialReferenced[materialIndex] = true;

            uint32_t albedoIdx = (uint32_t)((*matSlots)[materialIndex].albedoTexIdx);
            textureReferenced[albedoIdx] = true;

            uint32_t normalTex = (*matSlots)[materialIndex].normalTexIdx;

            if (normalTex != ~0u) {
                textureReferenced[normalTex] = true;
            }

            uint32_t heightmapTex = (*matSlots)[materialIndex].heightmapTexIdx;

            if (heightmapTex != ~0u) {
                textureReferenced[heightmapTex] = true;
            }

            uint32_t metalMapTex = (*matSlots)[materialIndex].metalTexIdx;

            if (metalMapTex != ~0u) {
                textureReferenced[metalMapTex] = true;
            }

            uint32_t roughTexIdx = (*matSlots)[materialIndex].roughTexIdx;

            if (roughTexIdx != ~0u) {
                textureReferenced[roughTexIdx] = true;
            }

            uint32_t aoTexIdx = (*matSlots)[materialIndex].aoTexIdx;

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

    reg.view<WorldObject>().each([&referencedMeshes](WorldObject& wo) {
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

    robin_hood::unordered_set<AssetID> referencedCubemaps;

    reg.view<WorldCubemap>().each([&referencedCubemaps](WorldCubemap& wc) {
        referencedCubemaps.insert(wc.cubemapId);
        });

    referencedCubemaps.insert(reg.ctx<SceneSettings>().skybox);

    for (int i = 0; i < NUM_CUBEMAP_SLOTS; i++) {
        if (cubemapSlots->isSlotPresent(i)) {
            AssetID id = cubemapSlots->getKeyForSlot(i);

            if (!referencedCubemaps.contains(id)) {
                logMsg("unloading %s", AssetDB::idToPath(id).c_str());
                cubemapSlots->unload(i);
            }
        }
    }

    reuploadMaterials();
}

void VKRenderer::reloadContent(ReloadFlags flags) {
    VKCHECK(vkDeviceWaitIdle(device));
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

    if (enumHasFlag(flags, ReloadFlags::Materials)) {
        for (uint32_t i = 0; i < NUM_MAT_SLOTS; i++) {
            if (matSlots->isSlotPresent(i))
                matSlots->unload(i);
        }
    }

    if (enumHasFlag(flags, ReloadFlags::Meshes)) {
        loadedMeshes.clear();
    }
}

void VKRenderer::setVsync(bool vsync) {
    if (useVsync != vsync) {
        useVsync = vsync;
        recreateSwapchain();
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

void VKRenderer::setImGuiDrawData(void* drawData) {
    imDrawData = (ImDrawData*)drawData;
}

VKRTTPass* VKRenderer::createRTTPass(RTTPassCreateInfo& ci) {
    std::lock_guard<std::mutex> lg { *apiMutex };
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
    std::lock_guard<std::mutex> lg { *apiMutex };
    rttPasses.erase(
        std::remove(rttPasses.begin(), rttPasses.end(), pass),
        rttPasses.end());

    delete (VKRTTPass*)pass;
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

IUITextureManager& VKRenderer::uiTextureManager() {
    return *uiTextureMan;
}

namespace worlds { extern void unloadSDFFonts(); }

VKRenderer::~VKRenderer() {
    if (device) {
        VKCHECK(vkDeviceWaitIdle(device));
        auto physDevProps = getPhysicalDeviceProperties(physicalDevice);
        PipelineCacheSerializer::savePipelineCache(physDevProps, pipelineCache, device);

        ShaderCache::clear();


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

        unloadSDFFonts();

        delete shadowCascadePass;
        delete additionalShadowsPass;

        delete uiTextureMan;

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

        framebuffers.clear();

        for (int i = 0 ; i < presentSubmitManager->numFramesInFlight(); i++)
            DeletionQueue::cleanupFrame(i);

        presentSubmitManager.reset();

#ifndef NDEBUG
        char* statsString;
        vmaBuildStatsString(allocator, &statsString, true);

        FILE* file = fopen("memory_shutdown.json", "w");
        fwrite(statsString, strlen(statsString), 1, file);
        fclose(file);

        vmaFreeStatsString(allocator, statsString);
#endif
        vmaDestroyAllocator(allocator);

        vkDestroySurfaceKHR(instance, surface, nullptr);

        vkDestroyQueryPool(device, queryPool, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        vkDestroyDevice(device, nullptr);
        dbgCallback.reset();
        vkDestroyInstance(instance, nullptr);
        logVrb(WELogCategoryRender, "Renderer destroyed.");
    }
}
