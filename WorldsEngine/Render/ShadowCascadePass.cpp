#include "RenderPasses.hpp"
#include "../Core/Engine.hpp"
#include "../Core/Transform.hpp"
#include "Render.hpp"
#include "Frustum.hpp"
#include "tracy/Tracy.hpp"
#include "ShaderCache.hpp"
#include "vku/RenderpassMaker.hpp"
#include "vku/PipelineMakers.hpp"
#include "vku/DescriptorSetUtil.hpp"

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
        dslm.buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
        dsl = dslm.create(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(dsl);
        ds = dsm.create(handles->device, handles->descriptorPool)[0];
    }

    void ShadowCascadePass::createRenderPass() {
        vku::RenderpassMaker rPassMaker;

        rPassMaker.attachmentBegin(VK_FORMAT_D32_SFLOAT);
        rPassMaker.attachmentLoadOp(VK_ATTACHMENT_LOAD_OP_CLEAR);
        rPassMaker.attachmentStencilLoadOp(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
        rPassMaker.attachmentFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        rPassMaker.subpassBegin(VK_PIPELINE_BIND_POINT_GRAPHICS);
        rPassMaker.subpassDepthStencilAttachment(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0);

        rPassMaker.dependencyBegin(0, 0);
        rPassMaker.dependencyDependencyFlags(VK_DEPENDENCY_VIEW_LOCAL_BIT | VK_DEPENDENCY_BY_REGION_BIT);
        rPassMaker.dependencySrcStageMask(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        rPassMaker.dependencyDstStageMask(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        rPassMaker.dependencyDstAccessMask(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        VkRenderPassMultiviewCreateInfo multiviewCI{ VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO };
        uint32_t viewMask = 0b00000111;

        multiviewCI.subpassCount = 1;
        multiviewCI.pViewMasks = &viewMask;
        multiviewCI.correlationMaskCount = 1;
        multiviewCI.pCorrelationMasks = &viewMask;

        rPassMaker.setPNext(&multiviewCI);

        renderPass = rPassMaker.create(handles->device);
    }

    void ShadowCascadePass::setup() {
        ZoneScoped;
        shadowmapRes = handles->graphicsSettings.shadowmapRes;

        createDescriptorSet();
        createRenderPass();

        AssetID vsID = AssetDB::pathToId("Shaders/shadow_cascade.vert.spv");
        AssetID fsID = AssetDB::pathToId("Shaders/blank.frag.spv");
        shadowVertexShader = ShaderCache::getModule(handles->device, vsID);
        shadowFragmentShader = ShaderCache::getModule(handles->device, fsID);

        vku::PipelineLayoutMaker plm{};
        plm.descriptorSetLayout(dsl);
        plm.pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowmapPushConstants));
        pipelineLayout = plm.create(handles->device);

        vku::PipelineMaker pm{ shadowmapRes, shadowmapRes };
        pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, shadowFragmentShader);
        pm.shader(VK_SHADER_STAGE_VERTEX_BIT, shadowVertexShader);
        pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
        pm.vertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));
        pm.cullMode(VK_CULL_MODE_BACK_BIT);
        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(VK_COMPARE_OP_GREATER);

        pipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout, renderPass);

        auto attachment = shadowImage->image.imageView();

        VkFramebufferCreateInfo fci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fci.attachmentCount = 1;
        fci.pAttachments = &attachment;
        fci.width = fci.height = shadowmapRes;
        fci.renderPass = renderPass;
        fci.layers = 1;

        VKCHECK(vkCreateFramebuffer(handles->device, &fci, nullptr, &shadowFb));

        matrixBuffer = vku::UniformBuffer {
            handles->device, handles->allocator, sizeof(CascadeMatrices), VMA_MEMORY_USAGE_GPU_ONLY, "Cascade Matrices"
        };

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(ds);
        dsu.beginBuffers(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        dsu.buffer(matrixBuffer.buffer(), 0, sizeof(CascadeMatrices));
        dsu.update(handles->device);
    }

    void ShadowCascadePass::prePass(RenderContext& ctx) {
    }

    void ShadowCascadePass::execute(RenderContext& ctx) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        TracyVkZone((*ctx.debugContext.tracyContexts)[ctx.imageIndex], ctx.cmdBuf, "Shadowmap");
#endif
        auto cmdBuf = ctx.cmdBuf;
        VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        label.pLabelName = "Cascade Shadows Pass";
        label.color[0] = 0.909f;
        label.color[1] = 0.764f;
        label.color[2] = 0.447f;
        label.color[3] = 1.0f;
        vkCmdBeginDebugUtilsLabelEXT(cmdBuf, &label);

        matrixBuffer.barrier(
            ctx.cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);


        VkClearDepthStencilValue clearDepthValue{ 0.0f, 0 };

        VkRenderPassBeginInfo rpbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };

        rpbi.renderPass = renderPass;
        rpbi.framebuffer = shadowFb;
        rpbi.renderArea = VkRect2D{ {0, 0}, {shadowmapRes, shadowmapRes} };
        rpbi.clearValueCount = 1;
        rpbi.pClearValues = reinterpret_cast<VkClearValue*>(&clearDepthValue);

        entt::registry& reg = ctx.registry;

        CascadeMatrices matrices;

        for (int i = 0; i < 3; i++) {
            matrices.matrices[i] = ctx.cascadeInfo.matrices[i];
        }

        vkCmdUpdateBuffer(cmdBuf, matrixBuffer.buffer(), 0, sizeof(matrices), &matrices);

        vkCmdBeginRenderPass(cmdBuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &ds, 0, nullptr);

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

            vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(spc), &spc);

            VkBuffer vb = meshPos->second.vb.buffer();
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vb, &offset);
            vkCmdBindIndexBuffer(cmdBuf, meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
            vkCmdDrawIndexed(cmdBuf, meshPos->second.indexCount, 1, 0, 0, 0);
            ctx.debugContext.stats->numDrawCalls++;
            ctx.debugContext.stats->numTriangles += meshPos->second.indexCount / 3;
        });

        vkCmdEndRenderPass(cmdBuf);

        shadowImage->image.setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        vkCmdEndDebugUtilsLabelEXT(cmdBuf);
    }

    ShadowCascadePass::~ShadowCascadePass() {
        vkFreeDescriptorSets(handles->device, handles->descriptorPool, 1, &ds);
        vkDestroyDescriptorSetLayout(handles->device, dsl, nullptr);
    }
}
