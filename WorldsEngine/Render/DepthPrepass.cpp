#include <tracy/Tracy.hpp>
#include "RenderPasses.hpp"
#include "ShaderCache.hpp"

namespace worlds {
    DepthPrepass::DepthPrepass(VulkanHandles* handles) : handles(handles) {}

    void DepthPrepass::setup(RenderContext& ctx, VkRenderPass renderPass, VkPipelineLayout layout) {
        ZoneScoped;

        pipelineVariants = new VKPipelineVariants{handles, true, layout, renderPass};

        this->layout = layout;
    }

    void DepthPrepass::prePass(RenderContext& ctx) {
    }

    struct StandardPushConstants {
        uint32_t modelMatrixIdx;
        uint32_t materialIdx;
        uint32_t vpIdx;
        uint32_t objectId;

        glm::vec3 cubemapExt;
        uint32_t skinningOffset;
        glm::vec4 cubemapPos;

        glm::vec4 texScaleOffset;

        glm::ivec3 screenSpacePickPos;
        uint32_t cubemapIdx;
    };

    void DepthPrepass::execute(RenderContext& ctx, slib::StaticAllocList<SubmeshDrawInfo>& drawInfo) {
        ZoneScoped;
        TracyVkZone((*ctx.debugContext.tracyContexts)[ctx.frameIndex], ctx.cmdBuf, "Depth Pre-Pass");

        auto& cmdBuf = ctx.cmdBuf;
        addDebugLabel(cmdBuf, "Depth Pre-Pass", 0.368f, 0.415f, 0.819f, 1.0f);

        VkPipeline lastPipeline =
            pipelineVariants->getPipeline(false, ctx.passSettings.msaaLevel, ShaderVariantFlags::None);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, lastPipeline);


        for (auto& sdi : drawInfo) {
            if (sdi.dontPrepass) continue;
            ShaderVariantFlags svf = ShaderVariantFlags::None;

            if (!sdi.opaque) {
                svf |= ShaderVariantFlags::AlphaTest;
            }

            if (sdi.skinned) {
                svf |= ShaderVariantFlags::Skinnning;
            }

            VkPipeline needsPipeline = pipelineVariants->getPipeline(false, ctx.passSettings.msaaLevel, svf);

            if (needsPipeline != lastPipeline) {
                vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, needsPipeline);
                lastPipeline = needsPipeline;
            }

            StandardPushConstants pushConst {
                .modelMatrixIdx = sdi.matrixIdx,
                .materialIdx = sdi.materialIdx,
                .vpIdx = 0,
                .objectId = (uint32_t)sdi.ent,
                .skinningOffset = sdi.boneMatrixOffset,
                .texScaleOffset = sdi.texScaleOffset,
                .screenSpacePickPos = glm::ivec3(0, 0, 0)
            };

            vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConst), &pushConst);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmdBuf, 0, 1, &sdi.vb, &offset);
            if (sdi.skinned) {
                vkCmdBindVertexBuffers(cmdBuf, 1, 1, &sdi.boneVB, &offset);
            }
            vkCmdBindIndexBuffer(cmdBuf, sdi.ib, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmdBuf, sdi.indexCount, 1, sdi.indexOffset, 0, 0);
            ctx.debugContext.stats->numDrawCalls++;
        }
        vkCmdEndDebugUtilsLabelEXT(cmdBuf);
    }

    DepthPrepass::~DepthPrepass() {
        delete pipelineVariants;
    }
}
