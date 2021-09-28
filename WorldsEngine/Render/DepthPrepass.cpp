#include <tracy/Tracy.hpp>
#include "RenderPasses.hpp"

namespace worlds {
    DepthPrepass::DepthPrepass(VulkanHandles* handles) : handles(handles) {}

    void DepthPrepass::setup(RenderContext& ctx, VkRenderPass renderPass, VkPipelineLayout layout) {
        ZoneScoped;

        AssetID vsID = AssetDB::pathToId("Shaders/depth_prepass.vert.spv");
        auto preVertexShader = vku::loadShaderAsset(handles->device, vsID);
        {
            AssetID fsID = AssetDB::pathToId("Shaders/blank.frag.spv");
            auto preFragmentShader = vku::loadShaderAsset(handles->device, fsID);
            vku::PipelineMaker pm{ ctx.passWidth, ctx.passHeight };

            pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, preFragmentShader);
            pm.shader(VK_SHADER_STAGE_VERTEX_BIT, preVertexShader);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(Vertex, uv));
            pm.cullMode(VK_CULL_MODE_BACK_BIT);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(VK_COMPARE_OP_GREATER);
            pm.blendBegin(false);
            pm.frontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE);

            pm.rasterizationSamples(vku::sampleCountFlags(ctx.passSettings.msaaSamples));
            pm.subPass(0);
            depthPrePipeline = pm.create(handles->device, handles->pipelineCache, layout, renderPass);
        }

        {
            AssetID alphaTestID = AssetDB::pathToId("Shaders/alpha_test_prepass.frag.spv");
            auto alphaTestFragment = vku::loadShaderAsset(handles->device, alphaTestID);
            vku::PipelineMaker pm{ ctx.passWidth, ctx.passHeight };

            pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, alphaTestFragment);
            pm.shader(VK_SHADER_STAGE_VERTEX_BIT, preVertexShader);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(Vertex, uv));
            pm.cullMode(VK_CULL_MODE_BACK_BIT);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(VK_COMPARE_OP_GREATER);
            pm.blendBegin(false);
            pm.frontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE);

            pm.rasterizationSamples(vku::sampleCountFlags(ctx.passSettings.msaaSamples));
            pm.alphaToCoverageEnable(true);
            pm.subPass(0);
            alphaTestPipeline = pm.create(handles->device, handles->pipelineCache, layout, renderPass);
        }
        this->layout = layout;
    }

    void DepthPrepass::prePass(RenderContext& ctx) {
    }

    struct StandardPushConstants {
        uint32_t modelMatrixIdx;
        uint32_t materialIdx;
        uint32_t vpIdx;
        uint32_t objectId;

        glm::vec4 cubemapExt;
        glm::vec4 cubemapPos;

        glm::vec4 texScaleOffset;

        glm::ivec3 screenSpacePickPos;
        uint32_t cubemapIdx;
    };

    void DepthPrepass::execute(RenderContext& ctx, slib::StaticAllocList<SubmeshDrawInfo>& drawInfo) {
        ZoneScoped;
        TracyVkZone((*ctx.debugContext.tracyContexts)[ctx.imageIndex], ctx.cmdBuf, "Depth Pre-Pass");

        auto& cmdBuf = ctx.cmdBuf;
        addDebugLabel(cmdBuf, "Depth Pre-Pass", 0.368f, 0.415f, 0.819f, 1.0f);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPrePipeline);

        bool switchedToAlphaTest = false;

        for (auto& sdi : drawInfo) {
            if (sdi.dontPrepass) continue;
            if (!sdi.opaque && !switchedToAlphaTest) {
                switchedToAlphaTest = true;
                vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, alphaTestPipeline);
            }

            assert(!(sdi.opaque && switchedToAlphaTest));

            StandardPushConstants pushConst {
                .modelMatrixIdx = sdi.matrixIdx,
                .materialIdx = sdi.materialIdx,
                .vpIdx = 0,
                .objectId = (uint32_t)sdi.ent,
                .texScaleOffset = sdi.texScaleOffset,
                .screenSpacePickPos = glm::ivec3(0, 0, 0)
            };

            vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConst), &pushConst);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmdBuf, 0, 1, &sdi.vb, &offset);
            vkCmdBindIndexBuffer(cmdBuf, sdi.ib, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmdBuf, sdi.indexCount, 1, sdi.indexOffset, 0, 0);
            ctx.debugContext.stats->numDrawCalls++;
        }
        vkCmdEndDebugUtilsLabelEXT(cmdBuf);
    }
}
