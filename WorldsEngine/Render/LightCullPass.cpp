#include "RenderPasses.hpp"
#include "ShaderCache.hpp"

namespace worlds {
#pragma pack(push, 16)
    struct LightCullPushConstants {
        uint32_t screenWidth;
        uint32_t screenHeight;
        uint32_t eyeIdx;
    };
#pragma pack(pop)

    LightCullPass::LightCullPass(VulkanHandles* handles) : handles {handles} {
    }

    void LightCullPass::setup(RenderContext& ctx, VkBuffer lightBuffer, VkBuffer lightTileBuffer, VkDescriptorPool descriptorPool) {
        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        dslm.buffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        dslm.buffer(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        dsl = dslm.create(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(dsl);
        descriptorSet = dsm.create(handles->device, descriptorPool)[0];

        vku::PipelineLayoutMaker plm;
        plm.descriptorSetLayout(dsl);
        plm.pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(LightCullPushConstants));
        pipelineLayout = plm.create(handles->device);

        shader = ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/light_cull.comp.spv"));

        vku::ComputePipelineMaker pm;
        pm.shader(VK_SHADER_STAGE_COMPUTE_BIT, shader);
        pipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(descriptorSet);

        dsu.beginBuffers(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        dsu.buffer(lightTileBuffer, 0, sizeof(LightTileBuffer));

        dsu.beginBuffers(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        dsu.buffer(lightBuffer, 0, sizeof(LightUB));

        dsu.beginBuffers(2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        dsu.buffer(ctx.resources.vpMatrixBuffer->buffer(), 0, sizeof(MultiVP));

        dsu.update(handles->device);
    }

    void LightCullPass::execute(RenderContext& ctx, int tileSize) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        TracyVkZone((*ctx.debugContext.tracyContexts)[ctx.imageIndex], ctx.cmdBuf, "Light Culling");
#endif
        auto& cmdBuf = ctx.cmdBuf;
        addDebugLabel(cmdBuf, "Light Culling", 1.0f, 0.0f, 0.0f, 1.0f);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        LightCullPushConstants lcpc {
            .screenWidth = ctx.passWidth,
            .screenHeight = ctx.passHeight,
            .eyeIdx = 0
        };

        vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(lcpc), &lcpc);

        const int xTiles = (ctx.passWidth + (tileSize - 1)) / tileSize;
        const int yTiles = (ctx.passHeight + (tileSize - 1)) / tileSize;

        vkCmdDispatch(cmdBuf, xTiles, yTiles, 1);

        if (ctx.passSettings.enableVR) {
            lcpc.eyeIdx = 1;
            vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(lcpc), &lcpc);
            vkCmdDispatch(cmdBuf, xTiles, yTiles, 1);
        }

        vkCmdEndDebugUtilsLabelEXT(cmdBuf);
    }
}
