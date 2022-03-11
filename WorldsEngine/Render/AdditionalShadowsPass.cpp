#include "Core/NameComponent.hpp"
#include "RenderPasses.hpp"
#include "Render/Frustum.hpp"
#include "ShaderCache.hpp"
#include "vku/RenderpassMaker.hpp"
#include "vku/DescriptorSetUtil.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>
#include <Core/ConVar.hpp>
#include <entt/entity/registry.hpp>

namespace worlds {
    ConVar enableSpotShadows { "r_enableSpotShadows", "1" };

    struct ShadowPushConstants {
        glm::mat4 mvp;
        uint32_t materialIdx;
    };

    AdditionalShadowsPass::AdditionalShadowsPass(VulkanHandles* handles) : handles(handles) {
    }

    void AdditionalShadowsPass::updateDescriptorSet(RenderResources resources) {
        vku::DescriptorSetUpdater dsu{1, 256};
        dsu.beginDescriptorSet(descriptorSet);

        for (uint32_t i = 0; i < resources.textures.size(); i++) {
            if (resources.textures.isSlotPresent(i)) {
                dsu.beginImages(0, i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                dsu.image(sampler, resources.textures[i].imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }

        dsu.beginBuffers(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        dsu.buffer(resources.materialBuffer->buffer(), 0, resources.materialBuffer->size());

        dsu.update(handles->device);

        dsUpdateNeeded = false;
    }

    void AdditionalShadowsPass::setup(RenderResources ctx) {
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

        vku::DescriptorSetLayoutMaker dslm;
        dslm.image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, NUM_TEX_SLOTS);
        dslm.bindFlag(0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
        dslm.buffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        dsl = dslm.create(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(dsl);
        descriptorSet = dsm.create(handles->device, handles->descriptorPool)[0];

        vku::PipelineLayoutMaker plm{};
        plm.pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ShadowPushConstants));
        plm.descriptorSetLayout(dsl);
        pipelineLayout = plm.create(handles->device);

        vku::SamplerMaker sm{};
        sm.magFilter(VK_FILTER_LINEAR).minFilter(VK_FILTER_LINEAR).mipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR).anisotropyEnable(true).maxAnisotropy(16.0f).maxLod(VK_LOD_CLAMP_NONE).minLod(0.0f);
        sampler = sm.create(handles->device);

        uint32_t spotRes = handles->graphicsSettings.spotShadowmapRes;

        {
            vku::PipelineMaker pm{ spotRes, spotRes };
            pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, shadowFragmentShader);
            pm.shader(VK_SHADER_STAGE_VERTEX_BIT, shadowVertexShader);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(Vertex, uv));
            pm.cullMode(VK_CULL_MODE_BACK_BIT);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(VK_COMPARE_OP_GREATER);
            pm.depthBiasEnable(true);
            pm.depthBiasConstantFactor(-1.4f);
            pm.depthBiasSlopeFactor(-1.75f);
            pm.dynamicState(VK_DYNAMIC_STATE_VIEWPORT);
            pm.dynamicState(VK_DYNAMIC_STATE_SCISSOR);

            pipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout, renderPass);
        }

        {
            auto shadowAlphaFragmentShader = ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/alpha_test_shadowmap.frag.spv"));

            vku::PipelineMaker pm{ spotRes, spotRes };
            pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, shadowAlphaFragmentShader);
            pm.shader(VK_SHADER_STAGE_VERTEX_BIT, shadowVertexShader);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(Vertex, uv));
            pm.cullMode(VK_CULL_MODE_BACK_BIT);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(VK_COMPARE_OP_GREATER);
            pm.depthBiasEnable(true);
            pm.depthBiasConstantFactor(-1.4f);
            pm.depthBiasSlopeFactor(-1.75f);
            pm.dynamicState(VK_DYNAMIC_STATE_VIEWPORT);
            pm.dynamicState(VK_DYNAMIC_STATE_SCISSOR);

            alphaTestPipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout, renderPass);
        }

        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
            int lightRes = spotRes >> i;
            VkFramebufferAttachmentsCreateInfo faci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO };
            faci.attachmentImageInfoCount = 1;

            VkFramebufferAttachmentImageInfo faii{ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO };
            faii.width = faii.height = lightRes;
            faii.layerCount = 1;
            faii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            faii.viewFormatCount = 1;
            VkFormat shadowFormat = VK_FORMAT_D32_SFLOAT;
            faii.pViewFormats = &shadowFormat;

            faci.pAttachmentImageInfos = &faii;

            VkFramebufferCreateInfo fci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            fci.attachmentCount = 1;
            fci.pAttachments = nullptr;
            fci.width = fci.height = lightRes;
            fci.renderPass = renderPass;
            fci.layers = 1;
            fci.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
            fci.pNext = &faci;

            VKCHECK(vku::createFramebuffer(handles->device, &fci, &fbs[i]));
        }

        updateDescriptorSet(ctx);
    }

    void AdditionalShadowsPass::prePass(RenderContext& ctx) {
        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
            renderIdx[i] = false;
        }

        glm::mat4 viewVp = ctx.projMatrices[0] * ctx.viewMatrices[0];
        ctx.registry.view<WorldLight, Transform>().each([&](WorldLight& light, Transform& t) {
            if (!light.enableShadows) return;

            Camera shadowCam;
            shadowCam.position = t.position;
            shadowCam.rotation = t.rotation;
            shadowCam.verticalFOV = light.spotCutoff * 2.0f;
            shadowCam.near = light.shadowNear;
            shadowCam.far = light.shadowFar;
            glm::mat4 shadowVP = shadowCam.getProjectMatrixNonInfinite(1.0f) * shadowCam.getViewMatrix();
            glm::mat4 invVP = glm::inverse(shadowVP);

            glm::vec3 viewPos = glm::inverse(ctx.viewMatrices[0])[3];
            glm::vec4 transformedViewPos = shadowVP * glm::vec4(viewPos, 1.0f);
            transformedViewPos /= transformedViewPos.w;

            if (transformedViewPos.x > -1.0f && transformedViewPos.x < 1.0f &&
                transformedViewPos.y > -1.0f && transformedViewPos.y < 1.0f &&
                transformedViewPos.z > 0.0f && transformedViewPos.z < 1.0f) {
                // we're inside the shadow frustum, max priority
                light.currentViewSpaceSize = 4.0f;
                return;
            }

            glm::vec3 boundingPoints[8] = {
                glm::vec3(-1.0f, -1.0f, 1e-5),
                glm::vec3(1.0f, -1.0f, 1e-5),
                glm::vec3(1.0f, 1.0f, 1e-5),
                glm::vec3(-1.0f, 1.0f, 1e-5),
                glm::vec3(-1.0f, -1.0f, 1.0f),
                glm::vec3(1.0f, -1.0f, 1.0f),
                glm::vec3(1.0f, 1.0f, 1.0f),
                glm::vec3(-1.0f, 1.0f, 1.0f),
            };

            glm::vec3 screenSpaceBoundingPoints[8];

            for (int i = 0; i < 8; i++) {
                glm::vec4 pointTransformed = invVP * glm::vec4(boundingPoints[i], 1.0f);
                glm::vec3 pointWS = glm::vec3(pointTransformed) / pointTransformed.w;
                drawSphere(pointWS, glm::quat{1.0f, 0.0f, 0.0f, 0.0f}, 0.1f);
                glm::vec4 pointViewTransformed = viewVp * glm::vec4(pointWS, 1.0f);
                glm::vec3 pointVS = glm::vec3(pointViewTransformed) / pointViewTransformed.w;
                // clamp to screen
                pointVS.x = glm::clamp(pointVS.x, -1.0f, 1.0f);
                pointVS.y = glm::clamp(pointVS.y, -1.0f, 1.0f);

                screenSpaceBoundingPoints[i] = pointVS;
            }

            float totalArea = 0.0f;

            // area of near plane

            glm::vec2 sizeDif = maxCamVS - minCamVS;
            light.currentViewSpaceSize = sizeDif.x * sizeDif.y;
            });

        ctx.registry.sort<WorldLight>([&](entt::entity ea, entt::entity eb) {
            WorldLight& a = ctx.registry.get<WorldLight>(ea);
            WorldLight& b = ctx.registry.get<WorldLight>(eb);
            return a.currentViewSpaceSize < b.currentViewSpaceSize;
            });

        uint32_t shadowIdx = 0;
        ctx.registry.view<WorldLight, Transform>().each([&](WorldLight& light, Transform& t) {
            if (light.enableShadows && enableSpotShadows.getInt() && shadowIdx < NUM_SHADOW_LIGHTS) {
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

        if (dsUpdateNeeded) {
            updateDescriptorSet(ctx.resources);
        }
    }

    void AdditionalShadowsPass::execute(RenderContext& ctx) {
        ZoneScoped;
        TracyVkZone((*ctx.debugContext.tracyContexts)[ctx.frameIndex], ctx.cmdBuf, "Additional Shadows");

        auto cmdBuf = ctx.cmdBuf;

        uint32_t spotRes = handles->graphicsSettings.spotShadowmapRes;
        VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        label.pLabelName = "Spotlight Shadow Pass";
        label.color[0] = 0.239f;
        label.color[1] = 0.239f;
        label.color[2] = 0.239f;
        label.color[3] = 1.0f;
        vkCmdBeginDebugUtilsLabelEXT(cmdBuf, &label);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        for (int i = 0; i < NUM_SHADOW_LIGHTS; i++) {
            if (!renderIdx[i]) {
                continue;
            }
            Frustum shadowFrustum;
            shadowFrustum.fromVPMatrix(shadowMatrices[i]);

            VkClearValue clearVal = vku::makeDepthStencilClearValue(0.0f, 0);

            VkRenderPassAttachmentBeginInfo attachmentBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO };
            attachmentBeginInfo.attachmentCount = 1;
            auto imgView = ctx.resources.additionalShadowImages[i]->image().imageView();
            attachmentBeginInfo.pAttachments = &imgView;

            int res = spotRes >> i;

            VkViewport vp{};
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            vp.x = 0.0f;
            vp.y = 0.0f;
            vp.width = res;
            vp.height = res;
            vkCmdSetViewport(cmdBuf, 0, 1, &vp);

            VkRect2D scissor{};
            scissor.extent.width = res;
            scissor.extent.height = res;
            vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

            VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            rpbi.renderPass = renderPass;
            rpbi.framebuffer = fbs[i];
            rpbi.renderArea = VkRect2D{ {0, 0}, {(uint32_t)res, (uint32_t)res} };
            rpbi.clearValueCount = 1;
            rpbi.pClearValues = &clearVal;
            rpbi.pNext = &attachmentBeginInfo;
            vkCmdBeginRenderPass(cmdBuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

            ctx.registry.view<Transform, WorldObject>().each([&](auto ent, Transform& transform, WorldObject& obj) {
                if (!obj.castShadows) return;
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


                for (int i = 0; i < meshPos->second.numSubmeshes; i++) {
                    auto& currSubmesh = meshPos->second.submeshes[i];
                    uint32_t materialIdx;
                    if (obj.presentMaterials[i])
                        materialIdx = ctx.resources.materials.get(obj.materials[i]);
                    else
                        materialIdx = ctx.resources.materials.get(obj.materials[0]);

                    ShadowPushConstants spc{
                        .mvp = mvp,
                        .materialIdx = materialIdx
                    };
                    vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spc), &spc);

                    bool opaque = ctx.resources.materials[spc.materialIdx].getCutoff() == 0.0f;

                    if (!opaque)
                        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, alphaTestPipeline);
                    else
                        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

                    VkBuffer vb = meshPos->second.vb.buffer();
                    VkDeviceSize offset = 0;
                    vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vb, &offset);
                    vkCmdBindIndexBuffer(cmdBuf, meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
                    vkCmdDrawIndexed(cmdBuf, currSubmesh.indexCount, 1, currSubmesh.indexOffset, 0, 0);
                    ctx.debugContext.stats->numDrawCalls++;
                    ctx.debugContext.stats->numTriangles += currSubmesh.indexCount / 3;
                }
            });

            vkCmdEndRenderPass(cmdBuf);
            ctx.resources.additionalShadowImages[i]->image().setCurrentLayout(
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        }

        vkCmdEndDebugUtilsLabelEXT(cmdBuf);
    }

    AdditionalShadowsPass::~AdditionalShadowsPass() {
        DeletionQueue::queueDescriptorSetFree(handles->descriptorPool, descriptorSet);
    }
}
