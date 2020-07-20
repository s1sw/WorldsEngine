#include "RenderPasses.hpp"
#include "Engine.hpp"
#include "Transform.hpp"

ShadowmapRenderPass::ShadowmapRenderPass(RenderImageHandle shadowImage)
    : shadowImage(shadowImage) {

}

RenderPassIO ShadowmapRenderPass::getIO() {
    RenderPassIO io;
    io.outputs = {
        {
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            shadowImage
        }
    };

    return io;
}

void ShadowmapRenderPass::setup(PassSetupCtx& ctx) {
    shadowmapRes = ctx.graphicsSettings.shadowmapRes;
    auto memoryProps = ctx.physicalDevice.getMemoryProperties();
    vku::DescriptorSetLayoutMaker dslm;
    dsl = dslm.createUnique(ctx.device);

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

    renderPass = rPassMaker.createUnique(ctx.device);

    AssetID vsID = g_assetDB.addAsset("Shaders/shadowmap.vert.spv");
    AssetID fsID = g_assetDB.addAsset("Shaders/shadowmap.frag.spv");
    shadowVertexShader = vku::loadShaderAsset(ctx.device, vsID);
    shadowFragmentShader = vku::loadShaderAsset(ctx.device, fsID);

    vku::DescriptorSetMaker dsm;
    dsm.layout(*dsl);
    descriptorSet = dsm.create(ctx.device, ctx.descriptorPool)[0];

    vku::PipelineLayoutMaker plm{};
    plm.descriptorSetLayout(*dsl);
    plm.pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
    pipelineLayout = plm.createUnique(ctx.device);

    vku::PipelineMaker pm{ shadowmapRes, shadowmapRes };
    pm.shader(vk::ShaderStageFlagBits::eFragment, shadowFragmentShader);
    pm.shader(vk::ShaderStageFlagBits::eVertex, shadowVertexShader);
    pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
    pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
    pm.cullMode(vk::CullModeFlagBits::eBack);
    pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);

    pipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout, *renderPass);

   /* vk::ImageCreateInfo ici;
    ici.arrayLayers = 1;
    ici.extent = vk::Extent3D{ shadowmapRes, shadowmapRes, 1 };
    ici.format = vk::Format::eD32Sfloat;
    ici.imageType = vk::ImageType::e2D;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    ici.mipLevels = 1;
    ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;

    RTResourceCreateInfo resourceCreateInfo{
        ici,
        vk::ImageViewType::e2D,
        vk::ImageAspectFlagBits::eDepth
    };
    shadowmapImage = createRTResource(resourceCreateInfo);*/

    std::array<vk::ImageView, 1> shadowmapAttachments = { ctx.rtResources.at(shadowImage).image.imageView() };
    vk::FramebufferCreateInfo fci;
    fci.attachmentCount = shadowmapAttachments.size();
    fci.pAttachments = shadowmapAttachments.data();
    fci.width = fci.height = shadowmapRes;
    fci.renderPass = *renderPass;
    fci.layers = 1;
    shadowFb = ctx.device.createFramebufferUnique(fci);
}

void ShadowmapRenderPass::execute(RenderCtx& ctx) {
#ifdef TRACY_ENABLE
    ZoneScoped;
    TracyVkZone(tracyContexts[ctx.imageIndex], *ctx.cmdBuf, "Shadowmap");
#endif
    vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
    std::array<vk::ClearValue, 1> clearColours{ clearDepthValue };

    vk::RenderPassBeginInfo rpbi;

    rpbi.renderPass = *renderPass;
    rpbi.framebuffer = *shadowFb;
    rpbi.renderArea = vk::Rect2D{ {0, 0}, {shadowmapRes, shadowmapRes} };
    rpbi.clearValueCount = (uint32_t)clearColours.size();
    rpbi.pClearValues = clearColours.data();

    vk::UniqueCommandBuffer& cmdBuf = ctx.cmdBuf;
    Camera& cam = ctx.cam;
    entt::registry& reg = ctx.reg;


    cmdBuf->beginRenderPass(rpbi, vk::SubpassContents::eInline);
    cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

    glm::mat4 shadowmapMatrix;
    glm::vec3 viewPos = cam.position;

    reg.view<WorldLight, Transform>().each([&shadowmapMatrix, &viewPos](auto ent, WorldLight& l, Transform& transform) {
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

            shadowmapMatrix = proj * view;
        }
        });

    reg.view<Transform, WorldObject>().each([this, &cmdBuf, &cam, &shadowmapMatrix, &ctx](auto ent, Transform& transform, WorldObject& obj) {
        auto meshPos = ctx.loadedMeshes.find(obj.mesh);

        if (meshPos == ctx.loadedMeshes.end()) {
            // Haven't loaded the mesh yet
            return;
        }

        glm::mat4 model = transform.getMatrix();
        glm::mat4 mvp = shadowmapMatrix * model;
        cmdBuf->pushConstants<glm::mat4>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, mvp);
        cmdBuf->bindVertexBuffers(0, meshPos->second.vb.buffer(), vk::DeviceSize(0));
        cmdBuf->bindIndexBuffer(meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
        cmdBuf->drawIndexed(meshPos->second.indexCount, 1, 0, 0, 0);
        });

    reg.view<Transform, ProceduralObject>().each([this, &cmdBuf, &cam, &shadowmapMatrix](auto ent, Transform& transform, ProceduralObject& obj) {
        if (!obj.visible) return;
        glm::mat4 model = transform.getMatrix();
        glm::mat4 mvp = shadowmapMatrix * model;
        cmdBuf->pushConstants<glm::mat4>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, mvp);
        cmdBuf->bindVertexBuffers(0, obj.vb.buffer(), vk::DeviceSize(0));
        cmdBuf->bindIndexBuffer(obj.ib.buffer(), 0, obj.indexType);
        cmdBuf->drawIndexed(obj.indexCount, 1, 0, 0, 0);
        });

    cmdBuf->endRenderPass();
}