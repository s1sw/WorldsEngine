#include "RenderPasses.hpp"
#include "../Core/Engine.hpp"
#include "../Core/Transform.hpp"
#include "Render.hpp"
#include "Frustum.hpp"
#include "tracy/Tracy.hpp"
#include "ShaderCache.hpp"

namespace worlds {
    struct ShadowmapPushConstants {
        glm::mat4 model;
    };

    struct CascadeMatrices {
        glm::mat4 matrices[3];
    };

    ShadowmapRenderPass::ShadowmapRenderPass(RenderTexture* shadowImage)
        : shadowImage(shadowImage) {
    }

    void ShadowmapRenderPass::createDescriptorSet(VulkanHandles& ctx) {
        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
        dsl = dslm.createUnique(ctx.device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        ds = std::move(dsm.createUnique(ctx.device, ctx.descriptorPool)[0]);
    }

    void ShadowmapRenderPass::createRenderPass(VulkanHandles& ctx) {
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

        vk::RenderPassMultiviewCreateInfo multiviewCI;
        uint32_t viewMask = 0b00000111;

        multiviewCI.subpassCount = 1;
        multiviewCI.pViewMasks = &viewMask;
        multiviewCI.correlationMaskCount = 1;
        multiviewCI.pCorrelationMasks = &viewMask;

        rPassMaker.setPNext(&multiviewCI);

        renderPass = rPassMaker.createUnique(ctx.device);
    }

    void ShadowmapRenderPass::setup(PassSetupCtx& psCtx) {
        ZoneScoped;
        auto& ctx = psCtx.vkCtx;
        shadowmapRes = ctx.graphicsSettings.shadowmapRes;

        createDescriptorSet(ctx);
        createRenderPass(ctx);

        AssetID vsID = g_assetDB.addOrGetExisting("Shaders/shadowmap.vert.spv");
        AssetID fsID = g_assetDB.addOrGetExisting("Shaders/shadowmap.frag.spv");
        shadowVertexShader = ShaderCache::getModule(ctx.device, vsID);
        shadowFragmentShader = ShaderCache::getModule(ctx.device, fsID);

        vku::PipelineLayoutMaker plm{};
        plm.descriptorSetLayout(*dsl);
        plm.pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(ShadowmapPushConstants));
        pipelineLayout = plm.createUnique(ctx.device);

        vku::PipelineMaker pm{ shadowmapRes, shadowmapRes };
        pm.shader(vk::ShaderStageFlagBits::eFragment, shadowFragmentShader);
        pm.shader(vk::ShaderStageFlagBits::eVertex, shadowVertexShader);
        pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
        pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
        pm.cullMode(vk::CullModeFlagBits::eBack);
        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);

        pipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout, *renderPass);

        auto attachment = shadowImage->image.imageView();

        vk::FramebufferCreateInfo fci;
        fci.attachmentCount = 1;
        fci.pAttachments = &attachment;
        fci.width = fci.height = shadowmapRes;
        fci.renderPass = *renderPass;
        fci.layers = 1;
        shadowFb = ctx.device.createFramebufferUnique(fci);

        matrixBuffer = vku::UniformBuffer {
            ctx.device, ctx.allocator, sizeof(CascadeMatrices), VMA_MEMORY_USAGE_CPU_TO_GPU, "Cascade Matrices"
        };

        matricesMapped = (CascadeMatrices*)matrixBuffer.map(ctx.device);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(*ds);
        dsu.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
        dsu.buffer(matrixBuffer.buffer(), 0, sizeof(CascadeMatrices));
        dsu.update(ctx.device);
    }

    void ShadowmapRenderPass::prePass(PassSetupCtx& ctx, RenderCtx& rCtx) {
        for (int i = 0; i < 3; i++) {
            matricesMapped->matrices[i] = rCtx.cascadeShadowMatrices[i];
        }

        matrixBuffer.invalidate(ctx.vkCtx.device);
        matrixBuffer.flush(ctx.vkCtx.device);
    }

    void ShadowmapRenderPass::execute(RenderCtx& ctx) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        TracyVkZone((*ctx.tracyContexts)[ctx.imageIndex], *ctx.cmdBuf, "Shadowmap");
#endif
        matrixBuffer.barrier(
            ctx.cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eVertexShader,
            vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eUniformRead,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);


        vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
        std::array<vk::ClearValue, 1> clearColours{ clearDepthValue };

        vk::RenderPassBeginInfo rpbi;

        rpbi.renderPass = *renderPass;
        rpbi.framebuffer = *shadowFb;
        rpbi.renderArea = vk::Rect2D{ {0, 0}, {shadowmapRes, shadowmapRes} };
        rpbi.clearValueCount = (uint32_t)clearColours.size();
        rpbi.pClearValues = clearColours.data();

        auto cmdBuf = ctx.cmdBuf;
        Camera& cam = *ctx.cam;
        entt::registry& reg = ctx.reg;

        cmdBuf.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *ds, nullptr);

        // cull using the outermost cascade matrix, as it includes the inner cascades
        glm::mat4 shadowmapMatrix = ctx.cascadeShadowMatrices[2];
        Frustum frustum;
        frustum.fromVPMatrix(shadowmapMatrix);

        reg.view<Transform, WorldObject>().each([&](auto ent, Transform& transform, WorldObject& obj) {
            auto meshPos = ctx.loadedMeshes.find(obj.mesh);

            if (meshPos == ctx.loadedMeshes.end()) {
                // Haven't loaded the mesh yet
                return;
            }

            float scaleMax = glm::max(transform.scale.x, glm::max(transform.scale.y, transform.scale.z));
            if (!frustum.containsSphere(transform.position, meshPos->second.sphereRadius * scaleMax)) {
                ctx.dbgStats->numCulledObjs++;
                return;
            }

            ShadowmapPushConstants spc;
            spc.model = transform.getMatrix();

            cmdBuf.pushConstants<ShadowmapPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, spc);
            cmdBuf.bindVertexBuffers(0, meshPos->second.vb.buffer(), vk::DeviceSize(0));
            cmdBuf.bindIndexBuffer(meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
            cmdBuf.drawIndexed(meshPos->second.indexCount, 1, 0, 0, 0);
            ctx.dbgStats->numDrawCalls++;
        });

        reg.view<Transform, ProceduralObject>().each([&](auto ent, Transform& transform, ProceduralObject& obj) {
            if (!obj.visible) return;
            glm::mat4 model = transform.getMatrix();
            glm::mat4 mvp = shadowmapMatrix * model;
            cmdBuf.pushConstants<glm::mat4>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, mvp);
            cmdBuf.bindVertexBuffers(0, obj.vb.buffer(), vk::DeviceSize(0));
            cmdBuf.bindIndexBuffer(obj.ib.buffer(), 0, obj.indexType);
            cmdBuf.drawIndexed(obj.indexCount, 1, 0, 0, 0);
            ctx.dbgStats->numDrawCalls++;
        });

        cmdBuf.endRenderPass();

        shadowImage->image.setCurrentLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    ShadowmapRenderPass::~ShadowmapRenderPass() {
        // EW EW EW EW EW EW EW
        matrixBuffer.unmap(shadowFb.getOwner());
    }
}
