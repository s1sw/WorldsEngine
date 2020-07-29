#include "RenderPasses.hpp"
#include "Engine.hpp"
#include "Transform.hpp"
#include "spirv_reflect.h"

struct StandardPushConstants {
    glm::vec4 pack0;
    glm::vec4 texScaleOffset;
    // (x: model matrix index, y: material index, z: specular cubemap index, w: object picking id)
    glm::ivec4 ubIndices;
    glm::ivec4 screenSpacePickPos;
};

struct PickingBuffer {
    uint32_t depth;
    uint32_t objectID;
    uint32_t lock;
    uint32_t doPicking;
};

PolyRenderPass::PolyRenderPass(
    RenderImageHandle depthStencilImage,
    RenderImageHandle polyImage,
    RenderImageHandle shadowImage,
    bool enablePicking)
    : depthStencilImage(depthStencilImage)
    , polyImage(polyImage)
    , shadowImage(shadowImage)
    , enablePicking(enablePicking)
    , pickX(0) 
    , pickY(0)
    , pickedEnt(UINT32_MAX) {

}

RenderPassIO PolyRenderPass::getIO() {
    RenderPassIO io;
    io.inputs = {
        {
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::AccessFlagBits::eShaderRead,
            shadowImage
        }
    };

    io.outputs = {
        {
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eColorAttachmentWrite,
            polyImage
        }
    };
    return io;
}

void PolyRenderPass::setup(PassSetupCtx& ctx) {
    auto memoryProps = ctx.physicalDevice.getMemoryProperties();

    vku::SamplerMaker sm{};
    sm.magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear).mipmapMode(vk::SamplerMipmapMode::eLinear).anisotropyEnable(true).maxAnisotropy(16.0f);
    albedoSampler = sm.createUnique(ctx.device);

    vku::SamplerMaker ssm{};
    ssm.magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear).mipmapMode(vk::SamplerMipmapMode::eLinear).compareEnable(true).compareOp(vk::CompareOp::eLessOrEqual);
    shadowSampler = ssm.createUnique(ctx.device);

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
    // Picking
    dslm.buffer(6, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment, 1);
    dslm.bindFlag(4, vk::DescriptorBindingFlagBits::ePartiallyBound);

    this->dsl = dslm.createUnique(ctx.device);

    vku::PipelineLayoutMaker plm;
    plm.pushConstantRange(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, sizeof(StandardPushConstants));
    plm.descriptorSetLayout(*this->dsl);
    this->pipelineLayout = plm.createUnique(ctx.device);

    this->vpUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(MultiVP), VMA_MEMORY_USAGE_CPU_TO_GPU, "VP");
    lightsUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(LightUB), VMA_MEMORY_USAGE_CPU_TO_GPU, "Lights");
    materialUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(MaterialsUB), VMA_MEMORY_USAGE_GPU_ONLY, "Materials");
    modelMatrixUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(ModelMatrices), VMA_MEMORY_USAGE_CPU_TO_GPU, "Model matrices");
    pickingBuffer = vku::GenericBuffer(ctx.device, ctx.allocator, vk::BufferUsageFlagBits::eStorageBuffer, sizeof(PickingBuffer), VMA_MEMORY_USAGE_GPU_TO_CPU, "Picking buffer");

    pickEvent = ctx.device.createEventUnique(vk::EventCreateInfo{});

    MaterialsUB materials;
    materials.materials[0] = { glm::vec4(0.0f, 0.02f, 0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f) };
    materials.materials[1] = { glm::vec4(0.0f, 0.02f, 1.0f, 0.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f) };
    materialUB.upload(ctx.device, memoryProps, ctx.commandPool, ctx.device.getQueue(ctx.graphicsQueueFamilyIdx, 0), materials);

    vku::DescriptorSetMaker dsm;
    dsm.layout(*this->dsl);
    descriptorSet = dsm.create(ctx.device, ctx.descriptorPool)[0];

    vku::DescriptorSetUpdater updater;
    updater.beginDescriptorSet(descriptorSet);

    updater.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
    updater.buffer(this->vpUB.buffer(), 0, sizeof(MultiVP));

    updater.beginBuffers(1, 0, vk::DescriptorType::eUniformBuffer);
    updater.buffer(lightsUB.buffer(), 0, sizeof(LightUB));

    updater.beginBuffers(2, 0, vk::DescriptorType::eUniformBuffer);
    updater.buffer(materialUB.buffer(), 0, sizeof(MaterialsUB));

    updater.beginBuffers(3, 0, vk::DescriptorType::eUniformBuffer);
    updater.buffer(modelMatrixUB.buffer(), 0, sizeof(ModelMatrices));

    for (int i = 0; i < 64; i++) {
        if (ctx.globalTexArray[i].present) {
            updater.beginImages(4, i, vk::DescriptorType::eCombinedImageSampler);
            updater.image(*albedoSampler, ctx.globalTexArray[i].tex.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }

    updater.beginImages(5, 0, vk::DescriptorType::eCombinedImageSampler);
    updater.image(*shadowSampler, ctx.rtResources.at(shadowImage).image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    updater.beginBuffers(6, 0, vk::DescriptorType::eStorageBuffer);
    updater.buffer(pickingBuffer.buffer(), 0, sizeof(PickingBuffer));

    updater.update(ctx.device);

    vku::RenderpassMaker rPassMaker;

    rPassMaker.attachmentBegin(vk::Format::eR16G16B16A16Sfloat);
    rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
    rPassMaker.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
    rPassMaker.attachmentSamples(vku::sampleCountFlags(ctx.graphicsSettings.msaaLevel));
    rPassMaker.attachmentFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    rPassMaker.attachmentBegin(vk::Format::eD32Sfloat);
    rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
    rPassMaker.attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    rPassMaker.attachmentSamples(vku::sampleCountFlags(ctx.graphicsSettings.msaaLevel));
    rPassMaker.attachmentFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    rPassMaker.subpassBegin(vk::PipelineBindPoint::eGraphics);
    rPassMaker.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);
    rPassMaker.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);

    rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
    rPassMaker.dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rPassMaker.dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rPassMaker.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

    this->renderPass = rPassMaker.createUnique(ctx.device);

    vk::ImageView attachments[2] = { ctx.rtResources.at(polyImage).image.imageView(), ctx.rtResources.at(depthStencilImage).image.imageView() };

    auto extent = ctx.rtResources.at(polyImage).image.info().extent;
    vk::FramebufferCreateInfo fci;
    fci.attachmentCount = 2;
    fci.pAttachments = attachments;
    fci.width = extent.width;
    fci.height = extent.height;
    fci.renderPass = *this->renderPass;
    fci.layers = false ? 2 : 1; // TODO!!!!!!!!!!!
    renderFb = ctx.device.createFramebufferUnique(fci);

    AssetID vsID = g_assetDB.addOrGetExisting("Shaders/standard.vert.spv");
    AssetID fsID = g_assetDB.addOrGetExisting("Shaders/standard.frag.spv");
    vertexShader = vku::loadShaderAsset(ctx.device, vsID);
    fragmentShader = vku::loadShaderAsset(ctx.device, fsID);

    vku::PipelineMaker pm{ extent.width, extent.height };

    vk::SpecializationMapEntry pickingEntry{ 0, 0, sizeof(bool) };
    vk::SpecializationInfo si;
    si.dataSize = sizeof(bool);
    si.mapEntryCount = 1;
    si.pMapEntries = &pickingEntry;
    si.pData = &enablePicking;

    pm.shader(vk::ShaderStageFlagBits::eFragment, fragmentShader, "main", &si);
    pm.shader(vk::ShaderStageFlagBits::eVertex, vertexShader);
    pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
    pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
    pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal));
    pm.vertexAttribute(2, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, tangent));
    pm.vertexAttribute(3, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
    pm.cullMode(vk::CullModeFlagBits::eBack);
    pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);
    pm.blendBegin(false);
    pm.frontFace(vk::FrontFace::eCounterClockwise);

    vk::PipelineMultisampleStateCreateInfo pmsci;
    pmsci.rasterizationSamples = (vk::SampleCountFlagBits)ctx.graphicsSettings.msaaLevel;
    pm.multisampleState(pmsci);
    this->pipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout, *renderPass);

    {
        AssetID wvsID = g_assetDB.addOrGetExisting("Shaders/wire_obj.vert.spv");
        AssetID wfsID = g_assetDB.addOrGetExisting("Shaders/wire_obj.frag.spv");
        wireVertexShader = vku::loadShaderAsset(ctx.device, wvsID);
        wireFragmentShader = vku::loadShaderAsset(ctx.device, wfsID);

        vku::PipelineMaker pm{ extent.width, extent.height };
        pm.shader(vk::ShaderStageFlagBits::eFragment, wireFragmentShader);
        pm.shader(vk::ShaderStageFlagBits::eVertex, wireVertexShader);
        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);
        pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
        pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
        pm.vertexAttribute(1, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
        pm.polygonMode(vk::PolygonMode::eLine);
        pm.lineWidth(2.0f);

        vk::PipelineMultisampleStateCreateInfo pmsci;
        pmsci.rasterizationSamples = (vk::SampleCountFlagBits)ctx.graphicsSettings.msaaLevel;
        pm.multisampleState(pmsci);

        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
        dslm.buffer(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
        dslm.buffer(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1);
        dslm.image(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 64);
        dslm.bindFlag(3, vk::DescriptorBindingFlagBits::ePartiallyBound);
        wireframeDsl = dslm.createUnique(ctx.device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*wireframeDsl);
        wireframeDescriptorSet = dsm.create(ctx.device, ctx.descriptorPool)[0];

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, sizeof(StandardPushConstants));
        plm.descriptorSetLayout(*wireframeDsl);
        wireframePipelineLayout = plm.createUnique(ctx.device);

        wireframePipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *wireframePipelineLayout, *renderPass);

        vku::DescriptorSetUpdater updater;
        updater.beginDescriptorSet(wireframeDescriptorSet);
        updater.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
        updater.buffer(vpUB.buffer(), 0, sizeof(MultiVP));
        updater.beginBuffers(1, 0, vk::DescriptorType::eUniformBuffer);
        updater.buffer(modelMatrixUB.buffer(), 0, sizeof(ModelMatrices));
        updater.beginBuffers(2, 0, vk::DescriptorType::eUniformBuffer);
        updater.buffer(materialUB.buffer(), 0, sizeof(MaterialsUB));
        
        for (int i = 0; i < 64; i++) {
            if (ctx.globalTexArray[i].present) {
                updater.beginImages(3, i, vk::DescriptorType::eCombinedImageSampler);
                updater.image(*albedoSampler, ctx.globalTexArray[i].tex.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            }
        }

        updater.update(ctx.device);
    }
}

void PolyRenderPass::prePass(PassSetupCtx& ctx, RenderCtx& rCtx) {
    auto tfWoView = rCtx.reg.view<Transform, WorldObject>();

    ModelMatrices* modelMatricesMapped = static_cast<ModelMatrices*>(modelMatrixUB.map(ctx.device));

    int matrixIdx = 0;
    rCtx.reg.view<Transform, WorldObject>().each([&matrixIdx, modelMatricesMapped](auto ent, Transform& t, WorldObject& wo) {
        if (matrixIdx == 1023)
            return;
        glm::mat4 m = t.getMatrix();
        modelMatricesMapped->modelMatrices[matrixIdx] = m;
        matrixIdx++;
        });

    rCtx.reg.view<Transform, ProceduralObject>().each([&matrixIdx, modelMatricesMapped](auto ent, Transform& t, ProceduralObject& po) {
        if (matrixIdx == 1023)
            return;
        glm::mat4 m = t.getMatrix();
        modelMatricesMapped->modelMatrices[matrixIdx] = m;
        matrixIdx++;
        });

    modelMatrixUB.unmap(ctx.device);

    MultiVP* vp = (MultiVP*)vpUB.map(ctx.device);
    vp->views[0] = rCtx.cam.getViewMatrix();
    vp->projections[0] = rCtx.cam.getProjectionMatrix((float)rCtx.width / (float)rCtx.height);
    vpUB.unmap(ctx.device);


    LightUB* lub = (LightUB*)lightsUB.map(ctx.device);
    glm::vec3 viewPos = rCtx.cam.position;

    int lightIdx = 0;
    rCtx.reg.view<WorldLight, Transform>().each([&lub, &lightIdx, &viewPos](auto ent, WorldLight& l, Transform& transform) {
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

            lub->shadowmapMatrix = proj * view;
        }

        lub->lights[lightIdx] = PackedLight{
            glm::vec4(l.color, (float)l.type),
            glm::vec4(lightForward, l.spotCutoff),
            glm::vec4(transform.position, 0.0f) };
        lightIdx++;
        });

    lub->pack0.x = lightIdx;
    lightsUB.unmap(ctx.device);

    if (enablePicking && ctx.device.getEventStatus(*pickEvent) == vk::Result::eEventSet) {
        PickingBuffer* pickBuf = (PickingBuffer*)pickingBuffer.map(ctx.device);
        pickedEnt = pickBuf->objectID;
        pickBuf->objectID = UINT32_MAX;
        pickBuf->depth = UINT32_MAX;
        pickBuf->doPicking = true;
        
        pickingBuffer.unmap(ctx.device);
        pickingBuffer.invalidate(ctx.device);
        pickingBuffer.flush(ctx.device);
    }
}

void PolyRenderPass::execute(RenderCtx& ctx) {
#ifdef TRACY_ENABLE
    ZoneScoped;
    TracyVkZone(tracyContexts[ctx.imageIndex], *ctx.cmdBuf, "Polys");
#endif
    // Fast path clear values for AMD
    std::array<float, 4> clearColorValue{ 0.0f, 0.0f, 0.0f, 1 };
    vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
    std::array<vk::ClearValue, 2> clearColours{ vk::ClearValue{clearColorValue}, clearDepthValue };
    vk::RenderPassBeginInfo rpbi;

    rpbi.renderPass = *renderPass;
    rpbi.framebuffer = *renderFb;
    rpbi.renderArea = vk::Rect2D{ {0, 0}, {ctx.width, ctx.height} };
    rpbi.clearValueCount = (uint32_t)clearColours.size();
    rpbi.pClearValues = clearColours.data();

    vk::UniqueCommandBuffer& cmdBuf = ctx.cmdBuf;
    entt::registry& reg = ctx.reg;
    Camera& cam = ctx.cam;

    vpUB.barrier(
        *cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eVertexShader,
        vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eUniformRead,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);

    lightsUB.barrier(
        *cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eUniformRead,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);

    pickingBuffer.barrier(*cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eHostRead, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);

    cmdBuf->beginRenderPass(rpbi, vk::SubpassContents::eInline);
    

    int matrixIdx = 0;

    cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *wireframePipelineLayout, 0, wireframeDescriptorSet, nullptr);
    cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *wireframePipeline);
    reg.view<Transform, WorldObject, UseWireframe>().each([this, &cmdBuf, &cam, &matrixIdx, &ctx](auto ent, Transform& transform, WorldObject& obj) {
        auto meshPos = ctx.loadedMeshes.find(obj.mesh);

        if (meshPos == ctx.loadedMeshes.end()) {
            // Haven't loaded the mesh yet
            return;
        }

        StandardPushConstants pushConst{ glm::vec4(cam.position, 0.0f), obj.texScaleOffset, glm::ivec4(matrixIdx, obj.materialIndex, 0, ent), glm::ivec4(pickX, pickY, 0, 0) };
        cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
        cmdBuf->bindVertexBuffers(0, meshPos->second.vb.buffer(), vk::DeviceSize(0));
        cmdBuf->bindIndexBuffer(meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
        cmdBuf->drawIndexed(meshPos->second.indexCount, 1, 0, 0, 0);
        matrixIdx++;
        });

    matrixIdx = 0;
    cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSet, nullptr);
    cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

    reg.view<Transform, WorldObject>().each([this, &cmdBuf, &cam, &matrixIdx, &ctx](auto ent, Transform& transform, WorldObject& obj) {
        auto meshPos = ctx.loadedMeshes.find(obj.mesh);

        if (meshPos == ctx.loadedMeshes.end()) {
            // Haven't loaded the mesh yet
            return;
        }

        StandardPushConstants pushConst{ glm::vec4(cam.position, 0.0f), obj.texScaleOffset, glm::ivec4(matrixIdx, obj.materialIndex, 0, ent), glm::ivec4(pickX, pickY, 0, 0) };
        cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
        cmdBuf->bindVertexBuffers(0, meshPos->second.vb.buffer(), vk::DeviceSize(0));
        cmdBuf->bindIndexBuffer(meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
        cmdBuf->drawIndexed(meshPos->second.indexCount, 1, 0, 0, 0);
        matrixIdx++;
        });

    reg.view<Transform, ProceduralObject>().each([this, &cmdBuf, &cam, &matrixIdx](auto ent, Transform& transform, ProceduralObject& obj) {
        if (!obj.visible) return;
        StandardPushConstants pushConst{ glm::vec4(cam.position, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f), glm::ivec4(matrixIdx, 0, 0, ent), glm::ivec4(pickX, pickY, 0, 0) };
        cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
        cmdBuf->bindVertexBuffers(0, obj.vb.buffer(), vk::DeviceSize(0));
        cmdBuf->bindIndexBuffer(obj.ib.buffer(), 0, obj.indexType);
        cmdBuf->drawIndexed(obj.indexCount, 1, 0, 0, 0);
        matrixIdx++;
        });

    cmdBuf->endRenderPass();
    cmdBuf->setEvent(*pickEvent, vk::PipelineStageFlagBits::eFragmentShader);
}

uint32_t PolyRenderPass::getPickedEntity() {
    return pickedEnt;
}

PolyRenderPass::~PolyRenderPass() {
}