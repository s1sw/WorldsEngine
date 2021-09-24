#include "RenderPasses.hpp"
#include "vku/SamplerMaker.hpp"

namespace worlds {
    struct SkyboxPushConstants {
        uint32_t vpIdx;
        uint32_t cubemapIdx;
        uint32_t pad0;
        uint32_t pad1;
    };

    void SkyboxPass::updateDescriptors(RenderContext& ctx, uint32_t loadedSkyId) {
        auto& cubemapSlots = ctx.resources.cubemaps;
        vku::DescriptorSetUpdater updater;
        updater.beginDescriptorSet(skyboxDs);
        updater.beginBuffers(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        updater.buffer(ctx.resources.vpMatrixBuffer->buffer(), 0, sizeof(MultiVP));

        updater.beginImages(1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        updater.image(sampler, cubemapSlots[loadedSkyId].imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        updater.update(handles->device);
    }

    SkyboxPass::SkyboxPass(VulkanHandles* handles) : handles(handles) {
    }

    void SkyboxPass::setup(RenderContext& ctx, VkRenderPass renderPass, VkDescriptorPool descriptorPool) {
        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
        dslm.image(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        skyboxDsl = dslm.create(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(skyboxDsl);
        skyboxDs = dsm.create(handles->device, descriptorPool)[0];

        vku::PipelineLayoutMaker skyboxPl{};
        skyboxPl.descriptorSetLayout(skyboxDsl);
        skyboxPl.pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SkyboxPushConstants));
        skyboxPipelineLayout = skyboxPl.create(handles->device);

        vku::PipelineMaker pm{ ctx.passWidth, ctx.passHeight };
        AssetID vsID = AssetDB::pathToId("Shaders/skybox.vert.spv");
        AssetID fsID = AssetDB::pathToId("Shaders/skybox.frag.spv");

        auto vert = vku::loadShaderAsset(handles->device, vsID);
        auto frag = vku::loadShaderAsset(handles->device, fsID);

        pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, frag);
        pm.shader(VK_SHADER_STAGE_VERTEX_BIT, vert);
        pm.topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(VK_COMPARE_OP_GREATER_OR_EQUAL);

        pm.rasterizationSamples(vku::sampleCountFlags(ctx.passSettings.msaaSamples));
        pm.subPass(1);

        skyboxPipeline = pm.create(handles->device, handles->pipelineCache, skyboxPipelineLayout, renderPass);

        vku::SamplerMaker sm;
        sampler = sm.create(handles->device);
        updateDescriptors(ctx, 0);
    }

    void SkyboxPass::prePass(RenderContext& ctx) {
        auto& sceneSettings = ctx.registry.ctx<SceneSettings>();

        uint32_t skyboxId = ctx.resources.cubemaps.loadOrGet(sceneSettings.skybox);
        VkImageView imgView = ctx.resources.cubemaps[skyboxId].imageView();
        if (skyboxId != lastSky || lastSkyImageView != imgView) {
            updateDescriptors(ctx, skyboxId);
            lastSky = skyboxId;
            lastSkyImageView = imgView;
        }
    }

    void SkyboxPass::execute(RenderContext& ctx) {
        ZoneScoped;
        TracyVkZone((*ctx.debugContext.tracyContexts)[ctx.imageIndex], ctx.cmdBuf, "Skybox");

        auto& cmdBuf = ctx.cmdBuf;
        addDebugLabel(cmdBuf, "Skybox Pass", 0.321f, 0.705f, 0.871f, 1.0f);

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipelineLayout, 0, 1, &skyboxDs, 0, nullptr);
        SkyboxPushConstants spc{ 0, 0, 0, 0 };
        vkCmdPushConstants(cmdBuf, skyboxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spc), &spc);
        vkCmdDraw(cmdBuf, 36, 1, 0, 0);
        vkCmdEndDebugUtilsLabelEXT(cmdBuf);
        ctx.debugContext.stats->numDrawCalls++;
    }
}
