#include "RenderPasses.hpp"
#include "Engine.hpp"
#include "Transform.hpp"

PolyRenderPass::PolyRenderPass(
    RenderImageHandle depthStencilImage,
    RenderImageHandle polyImage,
    RenderImageHandle shadowImage)
    : depthStencilImage(depthStencilImage)
    , polyImage(polyImage)
    , shadowImage(shadowImage) {

}

RenderPassIO PolyRenderPass::getIO() {
    RenderPassIO io;
    io.outputs = {
        {
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eColorAttachmentWrite,
            polyImage
        }
    };
    return io;
}

void PolyRenderPass::setup(PassSetupCtx& ctx, RenderCtx& rCtx) {
    auto memoryProps = ctx.physicalDevice.getMemoryProperties();

    vku::SamplerMaker sm{};
    sm.magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear).mipmapMode(vk::SamplerMipmapMode::eLinear);
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
    // Cubemaps
    dslm.image(6, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 64);
    dslm.bindFlag(4, vk::DescriptorBindingFlagBits::ePartiallyBound);

    this->dsl = dslm.createUnique(ctx.device);

    vku::PipelineLayoutMaker plm;
    plm.pushConstantRange(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, sizeof(StandardPushConstants));
    plm.descriptorSetLayout(*this->dsl);
    this->pipelineLayout = plm.createUnique(ctx.device);

    this->vpUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(MultiVP));
    lightsUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(LightUB));
    materialUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(MaterialsUB));
    modelMatrixUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(ModelMatrices), VMA_MEMORY_USAGE_CPU_TO_GPU);

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

    updater.beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler);
    updater.image(*albedoSampler, rCtx.globalTexArray[0].tex.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    updater.beginImages(4, 1, vk::DescriptorType::eCombinedImageSampler);
    updater.image(*albedoSampler, rCtx.globalTexArray[1].tex.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    updater.beginImages(5, 0, vk::DescriptorType::eCombinedImageSampler);
    updater.image(*shadowSampler, rCtx.rtResources.at(shadowImage).image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    updater.update(ctx.device);

    vku::RenderpassMaker rPassMaker;

    rPassMaker.attachmentBegin(vk::Format::eR16G16B16A16Sfloat);
    rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
    rPassMaker.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
    rPassMaker.attachmentSamples(vku::sampleCountFlags(ctx.graphicsSettings.msaaLevel));
    rPassMaker.attachmentFinalLayout(vk::ImageLayout::eGeneral);

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

    AssetID vsID = g_assetDB.addAsset("Shaders/test.vert.spv");
    AssetID fsID = g_assetDB.addAsset("Shaders/test.frag.spv");
    vertexShader = vku::loadShaderAsset(ctx.device, vsID);
    fragmentShader = vku::loadShaderAsset(ctx.device, fsID);
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

    MultiVP vp;
    vp.views[0] = rCtx.cam.getViewMatrix();
    vp.projections[0] = rCtx.cam.getProjectionMatrix((float)rCtx.width / (float)rCtx.height);
    LightUB lub;

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

            lub.shadowmapMatrix = proj * view;
        }

        lub.lights[lightIdx] = PackedLight{
            glm::vec4(l.color, (float)l.type),
            glm::vec4(lightForward, l.spotCutoff),
            glm::vec4(transform.position, 0.0f) };
        lightIdx++;
        });

    lub.pack0.x = lightIdx;
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

    cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSet, nullptr);
    cmdBuf->beginRenderPass(rpbi, vk::SubpassContents::eInline);
    cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

    int matrixIdx = 0;

    reg.view<Transform, WorldObject>().each([this, &cmdBuf, &cam, &matrixIdx, &ctx](auto ent, Transform& transform, WorldObject& obj) {
        auto meshPos = ctx.loadedMeshes.find(obj.mesh);

        if (meshPos == ctx.loadedMeshes.end()) {
            // Haven't loaded the mesh yet
            return;
        }

        StandardPushConstants pushConst{ glm::vec4(cam.position, 0.0f), obj.texScaleOffset, glm::ivec4(matrixIdx, obj.materialIndex, 0, 0) };
        cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
        cmdBuf->bindVertexBuffers(0, meshPos->second.vb.buffer(), vk::DeviceSize(0));
        cmdBuf->bindIndexBuffer(meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
        cmdBuf->drawIndexed(meshPos->second.indexCount, 1, 0, 0, 0);
        matrixIdx++;
        });

    reg.view<Transform, ProceduralObject>().each([this, &cmdBuf, &cam, &matrixIdx](auto ent, Transform& transform, ProceduralObject& obj) {
        if (!obj.visible) return;
        StandardPushConstants pushConst{ glm::vec4(cam.position, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f), glm::ivec4(matrixIdx, 0, 0, 0) };
        cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
        cmdBuf->bindVertexBuffers(0, obj.vb.buffer(), vk::DeviceSize(0));
        cmdBuf->bindIndexBuffer(obj.ib.buffer(), 0, obj.indexType);
        cmdBuf->drawIndexed(obj.indexCount, 1, 0, 0, 0);
        matrixIdx++;
        });

    cmdBuf->endRenderPass();
}