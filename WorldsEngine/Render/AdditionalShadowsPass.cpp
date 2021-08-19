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

        rpm.attachmentBegin(VK_FORMAT_D32_SFLOAT);
        rpm.attachmentLoadOp(VK_ATTACHMENT_LOAD_OP_CLEAR);
        rpm.attachmentStencilLoadOp(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
        rpm.attachmentFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        rpm.subpassBegin(VK_PIPELINE_BIND_POINT_GRAPHICS);
        rpm.subpassDepthStencilAttachment(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0);

        rpm.dependencyBegin(0, 0);
        rpm.dependencyDependencyFlags(VK_DEPENDENCY_VIEW_LOCAL_BIT | VK_DEPENDENCY_BY_REGION_BIT);
        rpm.dependencySrcStageMask(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        rpm.dependencyDstStageMask(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        rpm.dependencyDstAccessMask(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        renderPass = rpm.create(handles->device);

        AssetID vsID = AssetDB::pathToId("Shaders/shadowmap.vert.spv");
        AssetID fsID = AssetDB::pathToId("Shaders/blank.frag.spv");
        auto shadowVertexShader = ShaderCache::getModule(handles->device, vsID);
        auto shadowFragmentShader = ShaderCache::getModule(handles->device, fsID);

        vku::PipelineLayoutMaker plm{};
        plm.pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4));
        pipelineLayout = plm.create(handles->device);

        vku::PipelineMaker pm{ 512, 512 };
        pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, shadowFragmentShader);
        pm.shader(VK_SHADER_STAGE_VERTEX_BIT, shadowVertexShader);
        pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
        pm.vertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));
        pm.cullMode(VK_CULL_MODE_BACK_BIT);
        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(VK_COMPARE_OP_GREATER);
        pm.depthBiasEnable(true);
        pm.depthBiasConstantFactor(-1.4f);
        pm.depthBiasSlopeFactor(-1.75f);

        pipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout, renderPass);

        VkFramebufferAttachmentsCreateInfo faci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO };
        faci.attachmentImageInfoCount = 1;

        VkFramebufferAttachmentImageInfo faii{ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO };
        faii.width = faii.height = 512;
        faii.layerCount = 1;
        faii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        faii.viewFormatCount = 1;
        VkFormat shadowFormat = VK_FORMAT_D32_SFLOAT;
        faii.pViewFormats = &shadowFormat;

        faci.pAttachmentImageInfos = &faii;

        VkFramebufferCreateInfo fci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fci.attachmentCount = 1;
        fci.pAttachments = nullptr;
        fci.width = fci.height = 512;
        fci.renderPass = renderPass;
        fci.layers = 1;
        fci.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
        fci.pNext = &faci;

        VKCHECK(vkCreateFramebuffer(handles->device, &fci, nullptr, &fb));
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
                shadowCam.verticalFOV = light.spotCutoff * 2.0f;
                shadowCam.near = light.shadowNear;
                shadowCam.far = light.shadowFar;
                shadowMatrices[shadowIdx] = shadowCam.getProjectMatrixNonInfinite(1.0f) * shadowCam.getViewMatrix();
                shadowIdx++;
            } else {
                light.shadowmapIdx = ~0u;
            }
        });
    }

    void AdditionalShadowsPass::execute(RenderContext& ctx) {
        auto cmdBuf = ctx.cmdBuf;

        VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        label.pLabelName = "Spotlight Shadow Pass";
        label.color[0] = 0.239f;
        label.color[1] = 0.239f;
        label.color[2] = 0.239f;
        label.color[3] = 1.0f;
        vkCmdBeginDebugUtilsLabelEXT(cmdBuf, &label);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
            if (!renderIdx[i]) {
                continue;
            }
            Frustum shadowFrustum;
            shadowFrustum.fromVPMatrix(shadowMatrices[i]);

            VkClearValue clearVal;
            clearVal.depthStencil = { 0.0f, 0 };

            VkRenderPassAttachmentBeginInfo attachmentBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO };
            attachmentBeginInfo.attachmentCount = 1;
            auto imgView = ctx.resources.additionalShadowImages[i]->image.imageView();
            attachmentBeginInfo.pAttachments = &imgView;

            VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            rpbi.renderPass = renderPass;
            rpbi.framebuffer = fb;
            rpbi.renderArea = VkRect2D{ {0, 0}, {512, 512} };
            rpbi.clearValueCount = 1;
            rpbi.pClearValues = &clearVal;
            rpbi.pNext = &attachmentBeginInfo;
            vkCmdBeginRenderPass(cmdBuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

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
                vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp), &mvp);

                VkBuffer vb = meshPos->second.vb.buffer();
                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vb, &offset);
                vkCmdBindIndexBuffer(cmdBuf, meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
                vkCmdDrawIndexed(cmdBuf, meshPos->second.indexCount, 1, 0, 0, 0);
                ctx.debugContext.stats->numDrawCalls++;
                ctx.debugContext.stats->numTriangles += meshPos->second.indexCount / 3;
            });

            vkCmdEndRenderPass(cmdBuf);
            ctx.resources.additionalShadowImages[i]->image.setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        }

        vkCmdEndDebugUtilsLabelEXT(cmdBuf);
    }
}
