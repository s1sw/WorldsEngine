#include "RenderPasses.hpp"
#include "ShaderCache.hpp"
#include "vku/DescriptorSetUtil.hpp"
#include "ShaderReflector.hpp"

namespace worlds {
#pragma pack(push, 16)
    struct LightCullPushConstants {
        glm::mat4 invViewProj;
        uint32_t screenWidth;
        uint32_t screenHeight;
        uint32_t eyeIdx;
    };
#pragma pack(pop)

    LightCullPass::LightCullPass(VulkanHandles* handles, RenderResource* depthResource)
        : RenderPass(handles)
        , depthResource(depthResource) {
    }

    void LightCullPass::resizeInternalBuffers(RenderContext& ctx) {
        vku::DescriptorSetUpdater dsu;
        for (VkDescriptorSet ds : descriptorSet) {
            dsu.beginDescriptorSet(ds);

            dsu.beginImages(3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            dsu.image(sampler, depthResource->image().imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            dsu.update(handles->device);
        }
    }

    void LightCullPass::changeLightTileBuffers(RenderContext& ctx, vku::GenericBuffer& lightTileBuffer, VkBuffer lightTileLightCountBuffer) {
        vku::DescriptorSetUpdater dsu;
        for (VkDescriptorSet ds : descriptorSet) {
            dsu.beginDescriptorSet(ds);

            const int tileSize = LightUB::LIGHT_TILE_SIZE;
            const int xTiles = (ctx.passWidth + (tileSize - 1)) / tileSize;
            const int yTiles = (ctx.passHeight + (tileSize - 1)) / tileSize;
            int numLightTiles = xTiles * yTiles;

            if (ctx.passSettings.enableVr)
                numLightTiles *= 2;

            dsu.beginBuffers(4, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            dsu.buffer(lightTileLightCountBuffer, 0, sizeof(uint32_t) * numLightTiles);

            dsu.beginBuffers(5, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            dsu.buffer(lightTileBuffer.buffer(), 0, sizeof(LightingTile) * numLightTiles);

            dsu.update(handles->device);
        }

        this->lightTileBuffer = &lightTileBuffer;
    }

    void LightCullPass::setup(
            RenderContext& ctx,
            std::vector<vku::GenericBuffer>& lightBuffers, VkBuffer lightTileInfoBuffer,
            vku::GenericBuffer& lightTileBuffer, VkBuffer lightTileLightCountBuffer,
            VkDescriptorPool descriptorPool) {

        this->lightTileBuffer = &lightTileBuffer;

        int tileSize = LightUB::LIGHT_TILE_SIZE;
        const int xTiles = (ctx.passWidth + (tileSize - 1)) / tileSize;
        const int yTiles = (ctx.passHeight + (tileSize - 1)) / tileSize;
        int numLightTiles = xTiles * yTiles;

        if (ctx.passSettings.enableVr)
            numLightTiles *= 2;

        AssetID depthShaderMsaa = AssetDB::pathToId("Shaders/light_cull_tile_depth.comp.spv");
        AssetID depthShaderNoMsaa = AssetDB::pathToId("Shaders/light_cull_tile_depth_nomsaa.comp.spv");
        AssetID depthShaderID = ctx.passSettings.msaaLevel > 1 ? depthShaderMsaa : depthShaderNoMsaa;

        ShaderReflector reflector{depthShaderID};
        dsl = reflector.createDescriptorSetLayout(handles->device, 0);

        vku::PipelineLayoutMaker plm;
        plm.descriptorSetLayout(dsl);
        plm.pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(LightCullPushConstants));
        pipelineLayout = plm.create(handles->device);

        vku::SamplerMaker sm;
        sampler = sm.create(handles->device);

        {
            AssetID clearShader = AssetDB::pathToId("Shaders/light_cull_tile_clear.comp.spv");
            vku::ComputePipelineMaker pm;
            pm.shader(VK_SHADER_STAGE_COMPUTE_BIT, ShaderCache::getModule(handles->device, clearShader));
            clearPipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout);
        }

        {
            vku::ComputePipelineMaker pm;
            pm.shader(VK_SHADER_STAGE_COMPUTE_BIT, ShaderCache::getModule(handles->device, depthShaderID));
            depthPipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout);
        }

        {
            AssetID setupShader = AssetDB::pathToId("Shaders/light_cull_tile_setup.comp.spv");
            vku::ComputePipelineMaker pm;
            pm.shader(VK_SHADER_STAGE_COMPUTE_BIT, ShaderCache::getModule(handles->device, setupShader));
            setupPipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout);
        }

        {
            AssetID cullShader = AssetDB::pathToId("Shaders/light_cull_tile_cull.comp.spv");
            vku::ComputePipelineMaker pm;
            pm.shader(VK_SHADER_STAGE_COMPUTE_BIT, ShaderCache::getModule(handles->device, cullShader));
            cullPipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout);
        }

        vku::DescriptorSetMaker dsm;
        dsm.layout(dsl);
        dsm.layout(dsl);
        descriptorSet = dsm.create(handles->device, descriptorPool);

        vku::DescriptorSetUpdater dsu;
        int idx = 0;
        for (VkDescriptorSet ds : descriptorSet) {
            dsu.beginDescriptorSet(ds);

            dsu.beginBuffers(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            dsu.buffer(lightTileInfoBuffer, 0, sizeof(LightTileInfoBuffer));

            dsu.beginBuffers(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            dsu.buffer(lightBuffers[idx].buffer(), 0, sizeof(LightUB));

            dsu.beginBuffers(2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            dsu.buffer(ctx.resources.vpMatrixBuffer->buffer(), 0, sizeof(MultiVP));

            dsu.beginImages(3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            dsu.image(sampler, depthResource->image().imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            dsu.beginBuffers(4, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            dsu.buffer(lightTileLightCountBuffer, 0, sizeof(uint32_t) * numLightTiles);

            dsu.beginBuffers(5, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            dsu.buffer(lightTileBuffer.buffer(), 0, sizeof(LightingTile) * numLightTiles);
            idx++;
        }

        dsu.update(handles->device);
    }

    void LightCullPass::execute(RenderContext& ctx, int tileSize) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        TracyVkZone((*ctx.debugContext.tracyContexts)[ctx.frameIndex], ctx.cmdBuf, "Light Culling");
#endif

        auto& cmdBuf = ctx.cmdBuf;
        addDebugLabel(cmdBuf, "Light Culling", 1.0f, 0.0f, 0.0f, 1.0f);

        depthResource->image().setLayout(cmdBuf,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet[ctx.frameIndex], 0, nullptr);

        LightCullPushConstants lcpc{
            .invViewProj = (ctx.projMatrices[0] * ctx.viewMatrices[0]),
            .screenWidth = ctx.passWidth,
            .screenHeight = ctx.passHeight,
            .eyeIdx = 0
        };

        vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(lcpc), &lcpc);
        const int xTiles = (ctx.passWidth + (tileSize - 1)) / tileSize;
        const int yTiles = (ctx.passHeight + (tileSize - 1)) / tileSize;
        const int xTilesDispatch = ((xTiles + 15) / 16) + 1;
        const int yTilesDispatch = ((yTiles + 15) / 16) + 1;

        // 1. Clear shader
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, clearPipeline);
        vkCmdDispatch(cmdBuf, xTilesDispatch, yTilesDispatch, 1);
        lightTileBuffer->barrier(
            cmdBuf,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED
        );

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, depthPipeline);
        vkCmdDispatch(cmdBuf, xTiles, yTiles, 1);

        lightTileBuffer->barrier(
            cmdBuf,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED
        );

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, setupPipeline);
        vkCmdDispatch(cmdBuf, xTilesDispatch, yTilesDispatch, 1);

        lightTileBuffer->barrier(
            cmdBuf,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED
        );

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipeline);
        vkCmdDispatch(cmdBuf, xTiles, yTiles, 1);

        if (ctx.passSettings.enableVr) {
            lcpc.eyeIdx = 1;
            lcpc.invViewProj = glm::inverse(ctx.projMatrices[1] * ctx.viewMatrices[1]);
            vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(lcpc), &lcpc);
            vkCmdDispatch(cmdBuf, xTiles, yTiles, 1);
        }

        depthResource->image().setLayout(cmdBuf,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);

        vkCmdEndDebugUtilsLabelEXT(cmdBuf);
    }

    LightCullPass::~LightCullPass() {
    }
}
