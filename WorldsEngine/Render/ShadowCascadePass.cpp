#include "Core/ConVar.hpp"
#include "RenderPasses.hpp"
#include "../Core/Engine.hpp"
#include "../Core/Transform.hpp"
#include "Render.hpp"
#include "Frustum.hpp"
#include "tracy/Tracy.hpp"
#include "ShaderCache.hpp"
#include "vku/RenderpassMaker.hpp"
#include "vku/DescriptorSetUtil.hpp"

namespace worlds {
    const int NUM_CASCADES = 4;
    struct ShadowmapPushConstants {
        glm::mat4 model;
    };

    struct CascadeMatrices {
        glm::mat4 matrices[NUM_CASCADES];
    };

    ShadowCascadePass::ShadowCascadePass(IVRInterface* vrInterface, VulkanHandles* handles, RenderResource* shadowImage)
        : shadowImage(shadowImage)
        , handles(handles)
        , vrInterface(vrInterface) {
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

        rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
        rPassMaker.dependencySrcStageMask(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        rPassMaker.dependencyDstStageMask(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        rPassMaker.dependencySrcAccessMask(VK_ACCESS_SHADER_READ_BIT);
        rPassMaker.dependencyDstAccessMask(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
        rPassMaker.dependencySrcStageMask(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        rPassMaker.dependencyDstStageMask(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        rPassMaker.dependencySrcAccessMask(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        rPassMaker.dependencyDstAccessMask(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        rPassMaker.dependencyBegin(0, VK_SUBPASS_EXTERNAL);
        rPassMaker.dependencySrcStageMask(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        rPassMaker.dependencyDstStageMask(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        rPassMaker.dependencySrcAccessMask(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        rPassMaker.dependencyDstAccessMask(VK_ACCESS_SHADER_READ_BIT);

        VkRenderPassMultiviewCreateInfo multiviewCI{ VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO };
        uint32_t viewMask = 0b00001111;

        multiviewCI.subpassCount = 1;
        multiviewCI.pViewMasks = &viewMask;
        multiviewCI.correlationMaskCount = 1;
        multiviewCI.pCorrelationMasks = &viewMask;

        rPassMaker.setPNext(&multiviewCI);

        renderPass = rPassMaker.create(handles->device);
    }

    glm::mat4 getCascadeMatrix(RenderContext& rCtx, glm::vec3 lightDir, glm::mat4 frustumMatrix, float& texelsPerUnit) {
        glm::mat4 view = rCtx.viewMatrices[0];

        glm::mat4 vpInv = glm::inverse(frustumMatrix * view);

        glm::vec3 frustumCorners[8] = {
            glm::vec3(-1.0f,  1.0f, -1.0f),
            glm::vec3(1.0f,  1.0f, -1.0f),
            glm::vec3(1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f,  1.0f,  1.0f),
            glm::vec3(1.0f,  1.0f,  1.0f),
            glm::vec3(1.0f, -1.0f,  1.0f),
            glm::vec3(-1.0f, -1.0f,  1.0f),
        };

        for (int i = 0; i < 8; i++) {
            glm::vec4 transformed = vpInv * glm::vec4{ frustumCorners[i], 1.0f };
            transformed /= transformed.w;
            frustumCorners[i] = transformed;
        }

        glm::vec3 center{ 0.0f };

        for (int i = 0; i < 8; i++) {
            center += frustumCorners[i];
        }

        center /= 8.0f;

        float diameter = 0.0f;
        for (int i = 0; i < 8; i++) {
            float dist = glm::length(frustumCorners[i] - center);
            diameter = glm::max(diameter, dist);
        }
        float radius = diameter * 0.5f;

        texelsPerUnit = (float)rCtx.passSettings.shadowmapRes / diameter;

        glm::mat4 scaleMatrix = glm::scale(glm::mat4{ 1.0f }, glm::vec3{ texelsPerUnit });

        glm::mat4 lookAt = glm::lookAt(glm::vec3{ 0.0f }, lightDir, glm::vec3{ 0.0f, 1.0f, 0.0f });
        lookAt *= scaleMatrix;

        glm::mat4 lookAtInv = glm::inverse(lookAt);

        center = lookAt * glm::vec4{ center, 1.0f };
        center = glm::floor(center);
        center = lookAtInv * glm::vec4{ center, 1.0f };

        glm::vec3 eye = center + (lightDir * diameter);

        glm::mat4 viewMat = glm::lookAt(eye, center, glm::vec3{ 0.0f, 1.0f, 0.0f });
        glm::mat4 projMat = glm::orthoZO(-radius, radius, -radius, radius, radius * 20.0f, -radius * 20.0f);

        return projMat * viewMat;
    }

    ConVar shadowDistanceScalar{ "r_shadowDistanceScalar", "1.0" };
    void ShadowCascadePass::calculateCascadeMatrices(RenderContext& rCtx) {
        bool hasLight = false;
        rCtx.registry.view<WorldLight, Transform>().each([&](auto, WorldLight& l, Transform& transform) {
            glm::vec3 lightForward = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            if (l.type == LightType::Directional) {
                glm::mat4 frustumMatrices[NUM_CASCADES];
                float aspect = (float)rCtx.passWidth / (float)rCtx.passHeight;
                // frustum 0: near -> 10m
                // frustum 1: 10m  -> 50m
                // frustum 2: 50m -> 200m
                // frustum 3: 200m -> 700m
                float splits[5] = { 0.1f, 10.0f, 50.0f, 200.0f, 700.0f };
                if (!rCtx.passSettings.enableVr) {
                    for (int i = 1; i < NUM_CASCADES; i++) {
                        frustumMatrices[i - 1] = glm::perspective(
                            rCtx.camera.verticalFOV, aspect,
                            splits[i - 1] * shadowDistanceScalar.getFloat(), splits[i] * shadowDistanceScalar.getFloat()
                        );
                    }
                } else {
                    for (int i = 1; i < NUM_CASCADES; i++) {
                        frustumMatrices[i - 1] = vrInterface->getEyeProjectionMatrix(
                            Eye::LeftEye,
                            splits[i - 1] * shadowDistanceScalar.getFloat(), splits[i] * shadowDistanceScalar.getFloat()
                        );
                    }
                }

                for (int i = 0; i < NUM_CASCADES; i++) {
                    rCtx.cascadeInfo.matrices[i] =
                        getCascadeMatrix(
                            rCtx, lightForward,
                            frustumMatrices[i], rCtx.cascadeInfo.texelsPerUnit[i]
                        );
                }
                hasLight = true;
            }
            });
        rCtx.cascadeInfo.shadowCascadeNeeded = hasLight;
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

        auto attachment = shadowImage->image().imageView();

        VkFramebufferCreateInfo fci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fci.attachmentCount = 1;
        fci.pAttachments = &attachment;
        fci.width = fci.height = shadowmapRes;
        fci.renderPass = renderPass;
        fci.layers = 1;

        VKCHECK(vku::createFramebuffer(handles->device, &fci, &shadowFb));

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
        calculateCascadeMatrices(ctx);
    }

    void ShadowCascadePass::execute(RenderContext& ctx) {
        if (!ctx.cascadeInfo.shadowCascadeNeeded) return;
#ifdef TRACY_ENABLE
        ZoneScoped;
        TracyVkZone((*ctx.debugContext.tracyContexts)[ctx.frameIndex], ctx.cmdBuf, "Shadowmap");
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

        for (int i = 0; i < NUM_CASCADES; i++) {
            matrices.matrices[i] = ctx.cascadeInfo.matrices[i];
        }

        matrixBuffer.barrier(cmdBuf,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            VK_ACCESS_UNIFORM_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED
        );
        vkCmdUpdateBuffer(cmdBuf, matrixBuffer.buffer(), 0, sizeof(matrices), &matrices);
        matrixBuffer.barrier(cmdBuf,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED
        );

        vkCmdBeginRenderPass(cmdBuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &ds, 0, nullptr);

        Frustum shadowFrustums[NUM_CASCADES];

        for (int i = 0; i < NUM_CASCADES; i++) {
            shadowFrustums[i].fromVPMatrix(ctx.cascadeInfo.matrices[i]);
        }

        reg.view<Transform, WorldObject>().each([&](auto ent, Transform& transform, WorldObject& obj) {
            if (!obj.castShadows) return;
            auto meshPos = ctx.resources.meshes.find(obj.mesh);

            if (meshPos == ctx.resources.meshes.end()) {
                // Haven't loaded the mesh yet
                return;
            }

            float scaleMax = glm::max(transform.scale.x, glm::max(transform.scale.y, transform.scale.z));

            bool visible = false;

            for (int i = 0; i < NUM_CASCADES; i++) {
                //bool visibleInFrustum = shadowFrustums[i].containsSphere(transform.position, meshPos->second.sphereRadius * scaleMax);
                glm::vec3 mi = transform.transformPoint(meshPos->second.aabbMin * transform.scale);
                glm::vec3 ma = transform.transformPoint(meshPos->second.aabbMax * transform.scale);

                glm::vec3 aabbMin = glm::min(mi, ma);
                glm::vec3 aabbMax = glm::max(mi, ma);

                visible |= shadowFrustums[i].containsAABB(aabbMin, aabbMax);
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

        shadowImage->image().setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        vkCmdEndDebugUtilsLabelEXT(cmdBuf);
    }

    ShadowCascadePass::~ShadowCascadePass() {
        vkFreeDescriptorSets(handles->device, handles->descriptorPool, 1, &ds);
    }
}
