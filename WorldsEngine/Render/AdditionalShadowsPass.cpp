#include "Render/Frustum.hpp"
#include "ShaderCache.hpp"
#include "RenderPasses.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace worlds {
    ConVar enableSpotShadows { "r_enableSpotShadows", "1" };

    AdditionalShadowsPass::AdditionalShadowsPass(VulkanHandles* handles) : handles(handles) {
    }

    void AdditionalShadowsPass::setup() {
        vku::RenderpassMaker rpm;

        rpm.attachmentBegin(vk::Format::eD16Unorm);
        rpm.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
        rpm.attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        rpm.attachmentFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        rpm.subpassBegin(vk::PipelineBindPoint::eGraphics);
        rpm.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 0);

        rpm.dependencyBegin(0, 0);
        rpm.dependencyDependencyFlags(vk::DependencyFlagBits::eViewLocal | vk::DependencyFlagBits::eByRegion);
        rpm.dependencySrcStageMask(vk::PipelineStageFlagBits::eLateFragmentTests);
        rpm.dependencyDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests);
        rpm.dependencyDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

        renderPass = rpm.createUnique(handles->device);

        AssetID vsID = AssetDB::pathToId("Shaders/shadowmap.vert.spv");
        AssetID fsID = AssetDB::pathToId("Shaders/blank.frag.spv");
        auto shadowVertexShader = ShaderCache::getModule(handles->device, vsID);
        auto shadowFragmentShader = ShaderCache::getModule(handles->device, fsID);

        vku::PipelineLayoutMaker plm{};
        plm.pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
        pipelineLayout = plm.createUnique(handles->device);

        vku::PipelineMaker pm{ 512, 512 };
        pm.shader(vk::ShaderStageFlagBits::eFragment, shadowFragmentShader);
        pm.shader(vk::ShaderStageFlagBits::eVertex, shadowVertexShader);
        pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
        pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
        pm.cullMode(vk::CullModeFlagBits::eBack);
        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);

        pipeline = pm.createUnique(handles->device, handles->pipelineCache, *pipelineLayout, *renderPass);

        vk::FramebufferAttachmentsCreateInfo faci;
        faci.attachmentImageInfoCount = 1;

        vk::FramebufferAttachmentImageInfo faii;
        faii.width = faii.height = 512;
        faii.layerCount = 1;
        faii.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
        faii.viewFormatCount = 1;
        vk::Format shadowFormat = vk::Format::eD16Unorm;
        faii.pViewFormats = &shadowFormat;

        faci.pAttachmentImageInfos = &faii;

        vk::FramebufferCreateInfo fci;
        fci.attachmentCount = 1;
        fci.pAttachments = nullptr;
        fci.width = fci.height = 512;
        fci.renderPass = *renderPass;
        fci.layers = 1;
        fci.flags = vk::FramebufferCreateFlagBits::eImageless;
        fci.pNext = &faci;
        fb = handles->device.createFramebufferUnique(fci);
    }

    void AdditionalShadowsPass::prePass(RenderContext& ctx) {
        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
            renderIdx[i] = false;
        }

        uint32_t shadowIdx = 0;
        ctx.registry.view<WorldLight, Transform>().each([&](WorldLight& light, Transform& t) {
            if (light.enableShadows && enableSpotShadows.getInt()) {
                light.shadowmapIdx = shadowIdx;
                renderIdx[shadowIdx] = true;
                Camera shadowCam;
                shadowCam.position = t.position;
                shadowCam.rotation = t.rotation;
                shadowCam.verticalFOV = glm::radians(90.f);
                shadowMatrices[shadowIdx] = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f) * shadowCam.getViewMatrix();
                shadowIdx++;
            } else {
                light.shadowmapIdx = ~0u;
            }
        });
    }

    void AdditionalShadowsPass::execute(RenderContext& ctx) {
        auto cmdBuf = ctx.cmdBuf;
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
            if (!renderIdx[i]) {
                continue;
            }
            Frustum shadowFrustum;
            shadowFrustum.fromVPMatrix(shadowMatrices[i]);

            vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
            std::array<vk::ClearValue, 1> clearColours{ clearDepthValue };

            vk::RenderPassAttachmentBeginInfo attachmentBeginInfo;
            attachmentBeginInfo.attachmentCount = 1;
            auto imgView = ctx.resources.additionalShadowImages[i]->image.imageView();
            attachmentBeginInfo.pAttachments = &imgView;

            vk::RenderPassBeginInfo rpbi;

            rpbi.renderPass = *renderPass;
            rpbi.framebuffer = *fb;
            rpbi.renderArea = vk::Rect2D{ {0, 0}, {512, 512} };
            rpbi.clearValueCount = (uint32_t)clearColours.size();
            rpbi.pClearValues = clearColours.data();
            rpbi.pNext = &attachmentBeginInfo;
            cmdBuf.beginRenderPass(rpbi, vk::SubpassContents::eInline);

            ctx.registry.view<Transform, WorldObject>().each([&](auto ent, Transform& transform, WorldObject& obj) {
                auto meshPos = ctx.resources.meshes.find(obj.mesh);

                if (meshPos == ctx.resources.meshes.end()) {
                    // Haven't loaded the mesh yet
                    return;
                }

                float scaleMax = glm::max(transform.scale.x, glm::max(transform.scale.y, transform.scale.z));
                if (!shadowFrustum.containsSphere(transform.position, meshPos->second.sphereRadius * scaleMax)) {
                    ctx.debugContext.stats->numCulledObjs++;
                    return;
                }

                glm::mat4 mvp = shadowMatrices[i] * transform.getMatrix();
                cmdBuf.pushConstants<glm::mat4>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, mvp);
                cmdBuf.bindVertexBuffers(0, meshPos->second.vb.buffer(), vk::DeviceSize(0));
                cmdBuf.bindIndexBuffer(meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
                cmdBuf.drawIndexed(meshPos->second.indexCount, 1, 0, 0, 0);
                ctx.debugContext.stats->numDrawCalls++;
                ctx.debugContext.stats->numTriangles += meshPos->second.indexCount / 3;
            });

            cmdBuf.endRenderPass();
            ctx.resources.additionalShadowImages[i]->image.setCurrentLayout(vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eLateFragmentTests, vk::AccessFlagBits::eDepthStencilAttachmentWrite);
        }
    }
}
