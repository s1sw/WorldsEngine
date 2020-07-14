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

RenderImageHandle VKRenderer::createRTResource(RTResourceCreateInfo resourceCreateInfo) {
    auto memProps = physicalDevice.getMemoryProperties();
    RenderTextureResource rtr;
    rtr.image = vku::GenericImage{ *device, memProps, resourceCreateInfo.ici, resourceCreateInfo.viewType, resourceCreateInfo.aspectFlags, false };
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
    textures[index] = vku::TextureImage2D{ *device, memProps, (uint32_t)x, (uint32_t)y, 1, vk::Format::eR8G8B8A8Srgb };

    std::vector<uint8_t> albedoDat(dat, dat + (x * y * 4));

    textures[index].upload(*device, allocator, albedoDat, *commandPool, memProps, device->getQueue(graphicsQueueFamilyIdx, 0));
}

void VKRenderer::loadAlbedo() {
    loadTex("albedo.png", 0);
    loadTex("terrain.png", 1);
}

void VKRenderer::setupTonemapping() {
    vku::DescriptorSetLayoutMaker tonemapDslm;
    tonemapDslm.image(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
    tonemapDslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);

    tonemapDsl = tonemapDslm.createUnique(*device);

    tonemapShader = loadShaderAsset(g_assetDB.addAsset("Shaders/tonemap.comp.spv"));

    vku::PipelineLayoutMaker plm;
    plm.descriptorSetLayout(*tonemapDsl);

    tonemapPipelineLayout = plm.createUnique(*device);

    vku::ComputePipelineMaker cpm;
    cpm.shader(vk::ShaderStageFlagBits::eCompute, tonemapShader);
    vk::SpecializationMapEntry samplesEntry{ 0, 0, sizeof(int32_t) };
    vk::SpecializationInfo si;
    si.dataSize = sizeof(int32_t);
    si.mapEntryCount = 1;
    si.pMapEntries = &samplesEntry;
    si.pData = &numMSAASamples;
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
    rPassMaker.attachmentInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);

    rPassMaker.subpassBegin(vk::PipelineBindPoint::eGraphics);
    rPassMaker.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);

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
    imguiInit.MinImageCount = (uint32_t)swapchain->images.size();
    imguiInit.ImageCount = (uint32_t)swapchain->images.size();
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

    vku::SamplerMaker ssm{};
    ssm.magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear).mipmapMode(vk::SamplerMipmapMode::eLinear).compareEnable(true).compareOp(vk::CompareOp::eLessOrEqual);
    shadowSampler = ssm.createUnique(*device);

    vku::DescriptorSetLayoutMaker dslm;
    // VP
    dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
    // Lights
    dslm.buffer(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 1);
    // Materials
    dslm.buffer(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1);
    // Model matrices
    dslm.buffer(3, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
    // Textures
    dslm.image(4, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 64);
    // Shadowmap
    dslm.image(5, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
    // Cubemaps
    dslm.image(6, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 64);
    dslm.bindFlag(4, vk::DescriptorBindingFlagBits::ePartiallyBound);

    this->dsl = dslm.createUnique(*this->device);

    vku::PipelineLayoutMaker plm;
    plm.pushConstantRange(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, sizeof(StandardPushConstants));
    plm.descriptorSetLayout(*this->dsl);
    this->pipelineLayout = plm.createUnique(*this->device);

    this->vpUB = vku::UniformBuffer(*this->device, allocator, sizeof(MultiVP));
    lightsUB = vku::UniformBuffer(*this->device, allocator, sizeof(LightUB));
    materialUB = vku::UniformBuffer(*this->device, allocator, sizeof(MaterialsUB));
    modelMatrixUB = vku::UniformBuffer(*device, allocator, sizeof(ModelMatrices), VMA_MEMORY_USAGE_CPU_TO_GPU);

    MaterialsUB materials;
    materials.materials[0] = { glm::vec4(0.0f, 0.02f, 0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f) };
    materials.materials[1] = { glm::vec4(0.0f, 0.02f, 1.0f, 0.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f) };
    materialUB.upload(*device, memoryProps, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), materials);

    vku::DescriptorSetMaker dsm;
    dsm.layout(*this->dsl);
    this->descriptorSets = dsm.create(*this->device, *this->descriptorPool);

    vku::DescriptorSetUpdater updater;
    updater.beginDescriptorSet(this->descriptorSets[0]);

    updater.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
    updater.buffer(this->vpUB.buffer(), 0, sizeof(MultiVP));

    updater.beginBuffers(1, 0, vk::DescriptorType::eUniformBuffer);
    updater.buffer(lightsUB.buffer(), 0, sizeof(LightUB));

    updater.beginBuffers(2, 0, vk::DescriptorType::eUniformBuffer);
    updater.buffer(materialUB.buffer(), 0, sizeof(MaterialsUB));

    updater.beginBuffers(3, 0, vk::DescriptorType::eUniformBuffer);
    updater.buffer(modelMatrixUB.buffer(), 0, sizeof(ModelMatrices));

    updater.beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler);
    updater.image(*albedoSampler, textures[0].imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    updater.beginImages(4, 1, vk::DescriptorType::eCombinedImageSampler);
    updater.image(*albedoSampler, textures[1].imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    updater.beginImages(5, 0, vk::DescriptorType::eCombinedImageSampler);
    updater.image(*shadowSampler, rtResources.at(shadowmapImage).image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    updater.update(*this->device);

    vku::RenderpassMaker rPassMaker;

    rPassMaker.attachmentBegin(vk::Format::eR16G16B16A16Sfloat);
    rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
    rPassMaker.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
    rPassMaker.attachmentSamples(msaaSamples);
    rPassMaker.attachmentFinalLayout(vk::ImageLayout::eGeneral);

    rPassMaker.attachmentBegin(vk::Format::eD32Sfloat);
    rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
    rPassMaker.attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    rPassMaker.attachmentSamples(msaaSamples);
    rPassMaker.attachmentFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    rPassMaker.subpassBegin(vk::PipelineBindPoint::eGraphics);
    rPassMaker.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);
    rPassMaker.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);

    rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
    rPassMaker.dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rPassMaker.dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rPassMaker.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

    this->renderPass = rPassMaker.createUnique(*this->device);

    AssetID vsID = g_assetDB.addAsset("Shaders/test.vert.spv");
    AssetID fsID = g_assetDB.addAsset("Shaders/test.frag.spv");
    vertexShader = loadShaderAsset(vsID);
    fragmentShader = loadShaderAsset(fsID);
}

void VKRenderer::setupShadowPass() {
    auto memoryProps = physicalDevice.getMemoryProperties();
    vku::DescriptorSetLayoutMaker dslm;
    shadowmapDsl = dslm.createUnique(*device);

    vku::RenderpassMaker rPassMaker;

    rPassMaker.attachmentBegin(vk::Format::eD32Sfloat);
    rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
    rPassMaker.attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    rPassMaker.attachmentFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    rPassMaker.subpassBegin(vk::PipelineBindPoint::eGraphics);
    rPassMaker.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 0);

    rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
    rPassMaker.dependencySrcStageMask(vk::PipelineStageFlagBits::eLateFragmentTests);
    rPassMaker.dependencyDstStageMask(vk::PipelineStageFlagBits::eLateFragmentTests);
    rPassMaker.dependencyDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    shadowmapPass = rPassMaker.createUnique(*device);

    AssetID vsID = g_assetDB.addAsset("Shaders/shadowmap.vert.spv");
    AssetID fsID = g_assetDB.addAsset("Shaders/shadowmap.frag.spv");
    shadowVertexShader = loadShaderAsset(vsID);
    shadowFragmentShader = loadShaderAsset(fsID);

    vku::DescriptorSetMaker dsm;
    dsm.layout(*shadowmapDsl);
    shadowmapDescriptorSet = dsm.create(*device, *descriptorPool)[0];

    vku::PipelineLayoutMaker plm{};
    plm.descriptorSetLayout(*shadowmapDsl);
    plm.pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
    shadowmapPipelineLayout = plm.createUnique(*device);

    vku::PipelineMaker pm{ shadowmapRes, shadowmapRes };
    pm.shader(vk::ShaderStageFlagBits::eFragment, shadowFragmentShader);
    pm.shader(vk::ShaderStageFlagBits::eVertex, shadowVertexShader);
    pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
    pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
    pm.cullMode(vk::CullModeFlagBits::eBack);
    pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);

    shadowmapPipeline = pm.createUnique(*device, *pipelineCache, *shadowmapPipelineLayout, *shadowmapPass);

    vk::ImageCreateInfo ici;
    ici.arrayLayers = 1;
    ici.extent = vk::Extent3D{ shadowmapRes, shadowmapRes, 1 };
    ici.format = vk::Format::eD32Sfloat;
    ici.imageType = vk::ImageType::e2D;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    ici.mipLevels = 1;
    ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;

    //shadowmapImage = vku::GenericImage(*device, memoryProps, ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eDepth, false);
    RTResourceCreateInfo resourceCreateInfo{
        ici,
        vk::ImageViewType::e2D,
        vk::ImageAspectFlagBits::eDepth
    };
    shadowmapImage = createRTResource(resourceCreateInfo);

    std::array<vk::ImageView, 1> shadowmapAttachments = { rtResources.at(shadowmapImage).image.imageView() };
    vk::FramebufferCreateInfo fci;
    fci.attachmentCount = shadowmapAttachments.size();
    fci.pAttachments = shadowmapAttachments.data();
    fci.width = fci.height = shadowmapRes;
    fci.renderPass = *shadowmapPass;
    fci.layers = 1;
    shadowmapFb = device->createFramebufferUnique(fci);
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

    vk::PhysicalDeviceFeatures features;
    features.shaderStorageImageMultisample = true;
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
    setupShadowPass();
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

    vku::PipelineMaker pm{ this->width, this->height };
    pm.shader(vk::ShaderStageFlagBits::eFragment, fragmentShader);
    pm.shader(vk::ShaderStageFlagBits::eVertex, vertexShader);
    pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
    pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
    pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal));
    pm.vertexAttribute(2, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, tangent));
    pm.vertexAttribute(3, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
    pm.vertexAttribute(4, 0, vk::Format::eR32Sfloat, (uint32_t)offsetof(Vertex, ao));
    pm.cullMode(vk::CullModeFlagBits::eBack);
    pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);
    pm.blendBegin(false);
    pm.frontFace(vk::FrontFace::eCounterClockwise);
    vk::PipelineMultisampleStateCreateInfo pmsci;
    pmsci.rasterizationSamples = msaaSamples;
    pm.multisampleState(pmsci);
    this->pipeline = pm.createUnique(*this->device, *this->pipelineCache, *this->pipelineLayout, *this->renderPass);

    vk::ImageCreateInfo ici;
    ici.imageType = vk::ImageType::e2D;
    ici.extent = vk::Extent3D{ width, height, 1 };
    ici.arrayLayers = enableVR ? 2 : 1;
    ici.mipLevels = 1;
    ici.format = vk::Format::eR16G16B16A16Sfloat;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    ici.samples = msaaSamples;
    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
    if (rtResources.count(polyImage) != 0) {
        rtResources.erase(polyImage);
    }
    RTResourceCreateInfo polyCreateInfo{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };
    polyImage = createRTResource(polyCreateInfo);
    ici.format = vk::Format::eD32Sfloat;
    ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    RTResourceCreateInfo depthCreateInfo{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eDepth };
    depthStencilImage = createRTResource(depthCreateInfo);

    graphSolver.clear();
    {
        TextureUsage shadowmapOutUsage;
        shadowmapOutUsage.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
        shadowmapOutUsage.handle = shadowmapImage;
        shadowmapOutUsage.accessFlags = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        shadowmapOutUsage.stageFlags = vk::PipelineStageFlagBits::eLateFragmentTests;
        RenderNode shadowmapNode;
        shadowmapNode.outputs.push_back(shadowmapOutUsage);
        shadowmapNode.execute = [this](RenderCtx& ctx) { renderShadowmap(ctx); };
        graphSolver.addNode(shadowmapNode);
    }

    {
        TextureUsage shadowmapInUsage;
        shadowmapInUsage.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
        shadowmapInUsage.handle = shadowmapImage;
        shadowmapInUsage.accessFlags = vk::AccessFlagBits::eShaderRead;
        shadowmapInUsage.stageFlags = vk::PipelineStageFlagBits::eFragmentShader;
        TextureUsage polyImgOutUsage;
        polyImgOutUsage.handle = polyImage;
        polyImgOutUsage.layout = vk::ImageLayout::eGeneral;
        polyImgOutUsage.accessFlags = vk::AccessFlagBits::eColorAttachmentWrite;
        polyImgOutUsage.stageFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;

        RenderNode polyNode;
        polyNode.inputs.push_back(shadowmapInUsage);
        polyNode.outputs.push_back(polyImgOutUsage);
        polyNode.execute = [this](RenderCtx& ctx) { renderPolys(ctx); };
        graphSolver.addNode(polyNode);
    }

    {
        TextureUsage polyImgInUsage;
        polyImgInUsage.handle = polyImage;
        polyImgInUsage.layout = vk::ImageLayout::eGeneral;
        polyImgInUsage.accessFlags = vk::AccessFlagBits::eShaderRead;
        polyImgInUsage.stageFlags = vk::PipelineStageFlagBits::eComputeShader;
        RenderNode tonemapNode;
        tonemapNode.inputs.push_back(polyImgInUsage);
        tonemapNode.execute = [this](RenderCtx& ctx) { doTonemap(ctx); };
        graphSolver.addNode(tonemapNode);
    }

    ici.arrayLayers = 1;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.format = vk::Format::eR8G8B8A8Unorm;
    ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc;
    finalPrePresent = vku::GenericImage(*device, memoryProps, ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, false);

    vku::executeImmediately(*device, *commandPool, device->getQueue(graphicsQueueFamilyIdx, 0), [this](vk::CommandBuffer cmdBuf) {
        rtResources.at(polyImage).image.setLayout(cmdBuf, vk::ImageLayout::eColorAttachmentOptimal);
        finalPrePresent.setLayout(cmdBuf, vk::ImageLayout::eColorAttachmentOptimal);
        });

    vk::ImageView attachments[2] = { rtResources.at(polyImage).image.imageView(), rtResources.at(depthStencilImage).image.imageView() };
    vk::FramebufferCreateInfo fci;
    fci.attachmentCount = 2;
    fci.pAttachments = attachments;
    fci.width = this->width;
    fci.height = this->height;
    fci.renderPass = *this->renderPass;
    fci.layers = enableVR ? 2 : 1;
    renderFb = device->createFramebufferUnique(fci);

    vk::ImageView finalImageView = finalPrePresent.imageView();
    fci.attachmentCount = 1;
    fci.pAttachments = &finalImageView;
    fci.renderPass = *imguiRenderPass;
    fci.layers = 1;
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

glm::vec3 shadowOffset(0.0f, 0.0f, 0.001f);

void VKRenderer::updateTonemapDescriptors() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    vku::DescriptorSetUpdater dsu;
    dsu.beginDescriptorSet(tonemapDescriptorSet);

    dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
    dsu.image(*albedoSampler, finalPrePresent.imageView(), vk::ImageLayout::eGeneral);

    dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
    dsu.image(*albedoSampler, rtResources.at(polyImage).image.imageView(), vk::ImageLayout::eGeneral);

    dsu.update(*device);
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

    auto tfWoView = reg.view<Transform, WorldObject>();

    ModelMatrices* modelMatricesMapped = static_cast<ModelMatrices*>(modelMatrixUB.map(*device));

    int matrixIdx = 0;
    reg.view<Transform, WorldObject>().each([&matrixIdx, modelMatricesMapped](auto ent, Transform& t, WorldObject& wo) {
        if (matrixIdx == 1023)
            return;
        glm::mat4 m = t.getMatrix();
        modelMatricesMapped->modelMatrices[matrixIdx] = m;
        matrixIdx++;
        });

    reg.view<Transform, ProceduralObject>().each([&matrixIdx, modelMatricesMapped](auto ent, Transform& t, ProceduralObject& po) {
        if (matrixIdx == 1023)
            return;
        glm::mat4 m = t.getMatrix();
        modelMatricesMapped->modelMatrices[matrixIdx] = m;
        matrixIdx++;
        });

    modelMatrixUB.unmap(*device);

    MultiVP vp;
    vp.views[0] = cam.getViewMatrix();
    vp.projections[0] = cam.getProjectionMatrix((float)width / (float)height);
    LightUB lub;

    glm::vec3 viewPos = cam.position;

    int lightIdx = 0;
    reg.view<WorldLight, Transform>().each([&lub, &lightIdx, &viewPos](auto ent, WorldLight& l, Transform& transform) {
        glm::vec3 lightForward = glm::normalize(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        if (l.type == LightType::Directional) {
            const float SHADOW_DISTANCE = 50.0f;
            glm::vec3 shadowMapPos = glm::round(viewPos - (transform.rotation * glm::vec3(0.0f, 0.f, 50.0f)));
            glm::mat4 proj = glm::orthoZO(
                -SHADOW_DISTANCE, SHADOW_DISTANCE,
                -SHADOW_DISTANCE, SHADOW_DISTANCE,
                1.0f, 1000.f);

            glm::mat4 view = glm::lookAt(
                shadowMapPos,
                shadowMapPos - lightForward,
                glm::vec3(0.0f, 1.0f, 0.0));

            lub.shadowmapMatrix = proj * view;
        }

        lub.lights[lightIdx] = PackedLight{
            glm::vec4(l.color, (float)l.type),
            glm::vec4(lightForward, l.spotCutoff),
            glm::vec4(transform.position, 0.0f) };
        lightIdx++;
        });

    lub.pack0.x = lightIdx;

    auto& cmdBuf = cmdBufs[imageIndex];

    vk::CommandBufferBeginInfo cbbi;

    cmdBuf->begin(cbbi);
    cmdBuf->resetQueryPool(*queryPool, 0, 2);
    cmdBuf->writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0);
    cmdBuf->updateBuffer<LightUB>(lightsUB.buffer(), 0, lub);
    cmdBuf->updateBuffer<MultiVP>(vpUB.buffer(), 0, vp);

    vpUB.barrier(
        *cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eVertexShader,
        vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eUniformRead,
        graphicsQueueFamilyIdx, graphicsQueueFamilyIdx);

    lightsUB.barrier(
        *cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eUniformRead,
        graphicsQueueFamilyIdx, graphicsQueueFamilyIdx);

    std::vector<RenderNode> solvedNodes = graphSolver.solve();
    std::unordered_map<RenderImageHandle, vk::ImageAspectFlagBits> rtAspects;

    for (auto& pair : rtResources) {
        rtAspects.insert({ pair.first, pair.second.aspectFlags });
    }

    std::vector<std::vector<ImageBarrier>> barriers = graphSolver.createImageBarriers(solvedNodes, rtAspects);

    RenderCtx rCtx{ cmdBuf, reg, imageIndex, cam };

    for (int i = 0; i < solvedNodes.size(); i++) {
        auto& node = solvedNodes[i];
        // Put in barriers for this node
        for (auto& barrier : barriers[i])
            imageBarrier(*cmdBuf, barrier);

        node.execute(rCtx);
    }

    vku::transitionLayout(*cmdBuf, swapchain->images[imageIndex],
        vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer,
        vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite);

    ::imageBarrier(*cmdBuf, finalPrePresent.image(), vk::ImageLayout::eTransferSrcOptimal,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer);

    vk::ImageBlit imageBlit;
    imageBlit.srcOffsets[1] = imageBlit.dstOffsets[1] = { (int)width, (int)height, 1 };
    imageBlit.dstSubresource = imageBlit.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    cmdBuf->blitImage(
        finalPrePresent.image(), vk::ImageLayout::eTransferSrcOptimal,
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
        vert.ao = 1.0f;
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