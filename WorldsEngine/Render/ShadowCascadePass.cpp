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

    ShadowCascadePass::ShadowCascadePass(VulkanHandles* handles, RenderTexture* shadowImage)
        : shadowImage(shadowImage)
        , handles(handles) {
    }

    void ShadowCascadePass::createDescriptorSet() {
        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
        dsl = dslm.createUnique(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        ds = std::move(dsm.createUnique(handles->device, handles->descriptorPool)[0]);
    }

    void ShadowCascadePass::createRenderPass() {
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

        renderPass = rPassMaker.createUnique(handles->device);
    }

    void ShadowCascadePass::setup() {
        ZoneScoped;
        shadowmapRes = handles->graphicsSettings.shadowmapRes;

        createDescriptorSet();
        createRenderPass();

        AssetID vsID = g_assetDB.addOrGetExisting("Shaders/shadowmap.vert.spv");
        AssetID fsID = g_assetDB.addOrGetExisting("Shaders/blank.frag.spv");
        shadowVertexShader = ShaderCache::getModule(handles->device, vsID);
        shadowFragmentShader = ShaderCache::getModule(handles->device, fsID);

        vku::PipelineLayoutMaker plm{};
        plm.descriptorSetLayout(*dsl);
        plm.pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(ShadowmapPushConstants));
        pipelineLayout = plm.createUnique(handles->device);

        vku::PipelineMaker pm{ shadowmapRes, shadowmapRes };
        pm.shader(vk::ShaderStageFlagBits::eFragment, shadowFragmentShader);
        pm.shader(vk::ShaderStageFlagBits::eVertex, shadowVertexShader);
        pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
        pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
        pm.cullMode(vk::CullModeFlagBits::eBack);
        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);

        pipeline = pm.createUnique(handles->device, handles->pipelineCache, *pipelineLayout, *renderPass);

        auto attachment = shadowImage->image.imageView();

        vk::FramebufferCreateInfo fci;
        fci.attachmentCount = 1;
        fci.pAttachments = &attachment;
        fci.width = fci.height = shadowmapRes;
        fci.renderPass = *renderPass;
        fci.layers = 1;
        shadowFb = handles->device.createFramebufferUnique(fci);

        matrixBuffer = vku::UniformBuffer {
            handles->device, handles->allocator, sizeof(CascadeMatrices), VMA_MEMORY_USAGE_CPU_TO_GPU, "Cascade Matrices"
        };

        matricesMapped = (CascadeMatrices*)matrixBuffer.map(handles->device);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(*ds);
        dsu.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
        dsu.buffer(matrixBuffer.buffer(), 0, sizeof(CascadeMatrices));
        dsu.update(handles->device);
    }

    void ShadowCascadePass::prePass(RenderContext& ctx) {
        for (int i = 0; i < 3; i++) {
            matricesMapped->matrices[i] = ctx.cascadeInfo.matrices[i];
        }

        matrixBuffer.invalidate(handles->device);
        matrixBuffer.flush(handles->device);
    }

    void ShadowCascadePass::execute(RenderContext& ctx) {
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
        entt::registry& reg = ctx.registry;

        cmdBuf.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *ds, nullptr);

        Frustum shadowFrustums[3];

        for (int i = 0; i < 3; i++) {
            shadowFrustums[i].fromVPMatrix(ctx.cascadeInfo.matrices[i]);
        }

        reg.view<Transform, WorldObject>().each([&](auto ent, Transform& transform, WorldObject& obj) {
            auto meshPos = ctx.resources.meshes.find(obj.mesh);

            if (meshPos == ctx.resources.meshes.end()) {
                // Haven't loaded the mesh yet
                return;
            }

            float scaleMax = glm::max(transform.scale.x, glm::max(transform.scale.y, transform.scale.z));

            bool visible = false;

            for (int i = 0; i < 3; i++) {
                visible |= shadowFrustums[i].containsSphere(transform.position, meshPos->second.sphereRadius * scaleMax);
            }

            if (!visible) {
                ctx.debugContext.stats->numCulledObjs++;
                return;
            }

            ShadowmapPushConstants spc;
            spc.model = transform.getMatrix();

            cmdBuf.pushConstants<ShadowmapPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, spc);
            cmdBuf.bindVertexBuffers(0, meshPos->second.vb.buffer(), vk::DeviceSize(0));
            cmdBuf.bindIndexBuffer(meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
            cmdBuf.drawIndexed(meshPos->second.indexCount, 1, 0, 0, 0);
            ctx.debugContext.stats->numDrawCalls++;
        });

        reg.view<Transform, ProceduralObject>().each([&](auto ent, Transform& transform, ProceduralObject& obj) {
            if (!obj.visible) return;
            glm::mat4 model = transform.getMatrix();
            cmdBuf.pushConstants<glm::mat4>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, model);
            cmdBuf.bindVertexBuffers(0, obj.vb.buffer(), vk::DeviceSize(0));
            cmdBuf.bindIndexBuffer(obj.ib.buffer(), 0, obj.indexType);
            cmdBuf.drawIndexed(obj.indexCount, 1, 0, 0, 0);
            ctx.debugContext.stats->numDrawCalls++;
        });

        cmdBuf.endRenderPass();

        shadowImage->image.setCurrentLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    ShadowCascadePass::~ShadowCascadePass() {
        matrixBuffer.unmap(handles->device);
    }
}
