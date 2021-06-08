#include "RenderPasses.hpp"

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
        updater.beginDescriptorSet(*skyboxDs);
        updater.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
        updater.buffer(ctx.resources.vpMatrixBuffer->buffer(), 0, sizeof(MultiVP));

        updater.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
        updater.image(*sampler, cubemapSlots[loadedSkyId].imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        updater.update(handles->device);
    }

    SkyboxPass::SkyboxPass(VulkanHandles* handles) : handles(handles) {
    }

    void SkyboxPass::setup(RenderContext& ctx, vk::RenderPass renderPass, vk::DescriptorPool descriptorPool) {
        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
        dslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
        skyboxDsl = dslm.createUnique(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*skyboxDsl);
        skyboxDs = std::move(dsm.createUnique(handles->device, descriptorPool)[0]);

        vku::PipelineLayoutMaker skyboxPl{};
        skyboxPl.descriptorSetLayout(*skyboxDsl);
        skyboxPl.pushConstantRange(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, sizeof(SkyboxPushConstants));
        skyboxPipelineLayout = skyboxPl.createUnique(handles->device);

        vku::PipelineMaker pm{ ctx.passWidth, ctx.passHeight };
        AssetID vsID = AssetDB::pathToId("Shaders/skybox.vert.spv");
        AssetID fsID = AssetDB::pathToId("Shaders/skybox.frag.spv");

        auto vert = vku::loadShaderAsset(handles->device, vsID);
        auto frag = vku::loadShaderAsset(handles->device, fsID);

        pm.shader(vk::ShaderStageFlagBits::eFragment, frag);
        pm.shader(vk::ShaderStageFlagBits::eVertex, vert);
        pm.topology(vk::PrimitiveTopology::eTriangleList);
        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eGreaterOrEqual);

        vk::PipelineMultisampleStateCreateInfo pmsci;
        pmsci.rasterizationSamples = vku::sampleCountFlags(handles->graphicsSettings.msaaLevel);
        pm.multisampleState(pmsci);
        pm.subPass(1);

        skyboxPipeline = pm.createUnique(handles->device, handles->pipelineCache, *skyboxPipelineLayout, renderPass);

        vku::SamplerMaker sm;
        sampler = sm.createUnique(handles->device);
        updateDescriptors(ctx, 0);
    }

    void SkyboxPass::prePass(RenderContext& ctx) {
        auto& sceneSettings = ctx.registry.ctx<SceneSettings>();

        uint32_t skyboxId = ctx.resources.cubemaps.loadOrGet(sceneSettings.skybox);
        if (skyboxId != lastSky) {
            updateDescriptors(ctx, skyboxId);
            lastSky = skyboxId;
        }
    }

    void SkyboxPass::execute(RenderContext& ctx) {
        auto& cmdBuf = ctx.cmdBuf;
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *skyboxPipeline);
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *skyboxPipelineLayout, 0, *skyboxDs, nullptr);
        SkyboxPushConstants spc{ 0, 0, 0, 0 };
        cmdBuf.pushConstants<SkyboxPushConstants>(*skyboxPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, spc);
        cmdBuf.draw(36, 1, 0, 0);
        ctx.debugContext.stats->numDrawCalls++;
    }
}
