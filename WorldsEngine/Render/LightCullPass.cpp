#include "RenderPasses.hpp"
#include "ShaderCache.hpp"
#include "vku/SamplerMaker.hpp"

namespace worlds {
#pragma pack(push, 16)
    struct LightCullPushConstants {
        uint32_t screenWidth;
        uint32_t screenHeight;
        uint32_t eyeIdx;
    };
#pragma pack(pop)

    LightCullPass::LightCullPass(VulkanHandles* handles, RenderTexture* depthStencilImage)
        : handles{ handles }
        , depthStencilImage{ depthStencilImage } {
    }

    void LightCullPass::setup(RenderContext& ctx, VkBuffer lightBuffer, VkBuffer lightTileInfoBuffer, VkBuffer lightTilesBuffer, VkBuffer lightTileLightCountBuffer, VkDescriptorPool descriptorPool) {
        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        dslm.buffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        dslm.buffer(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        dslm.buffer(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        dslm.buffer(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        dslm.buffer(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        dsl = dslm.create(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(dsl);
        descriptorSet = dsm.create(handles->device, descriptorPool)[0];

        vku::PipelineLayoutMaker plm;
        plm.descriptorSetLayout(dsl);
        plm.pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(LightCullPushConstants));
        pipelineLayout = plm.create(handles->device);

        shader = ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/light_cull.comp.spv"));

        vku::SamplerMaker sm;
        sampler = sm.create(handles->device);

        vku::ComputePipelineMaker pm;
        pm.shader(VK_SHADER_STAGE_COMPUTE_BIT, shader);
        pipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(descriptorSet);

        dsu.beginBuffers(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        dsu.buffer(lightTileInfoBuffer, 0, sizeof(LightTileInfoBuffer));

        dsu.beginBuffers(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        dsu.buffer(lightBuffer, 0, sizeof(LightUB));

        dsu.beginBuffers(2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        dsu.buffer(ctx.resources.vpMatrixBuffer->buffer(), 0, sizeof(MultiVP));

        dsu.beginImages(3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        dsu.image(sampler, depthStencilImage->image.imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        dsu.beginBuffers(4, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        dsu.buffer(lightTileLightCountBuffer, 0, sizeof(uint32_t) * MAX_LIGHT_TILES);

        dsu.beginBuffers(5, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        dsu.buffer(lightTilesBuffer, 0, sizeof(LightingTile) * MAX_LIGHT_TILES);

        dsu.update(handles->device);
    }

    void LightCullPass::execute(RenderContext& ctx, int tileSize) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        TracyVkZone((*ctx.debugContext.tracyContexts)[ctx.imageIndex], ctx.cmdBuf, "Light Culling");
#endif
        auto& cmdBuf = ctx.cmdBuf;
        addDebugLabel(cmdBuf, "Light Culling", 1.0f, 0.0f, 0.0f, 1.0f);

        VkImageLayout oldLayout = depthStencilImage->image.layout();
        depthStencilImage->image.setLayout(cmdBuf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        LightCullPushConstants lcpc{
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

        depthStencilImage->image.setLayout(cmdBuf, oldLayout, VK_IMAGE_ASPECT_DEPTH_BIT);

        vkCmdEndDebugUtilsLabelEXT(cmdBuf);
    }

    LightCullPass::~LightCullPass() {
        vkDestroyPipeline(handles->device, pipeline, nullptr);
        vkDestroyPipelineLayout(handles->device, pipelineLayout, nullptr);
        vkDestroySampler(handles->device, sampler, nullptr);
        vkDestroyDescriptorSetLayout(handles->device, dsl, nullptr);
    }
}
