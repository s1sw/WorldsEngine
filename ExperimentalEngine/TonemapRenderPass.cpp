#include "RenderPasses.hpp"
#include "Engine.hpp"
#include "Render.hpp"

namespace worlds {
    struct TonemapPushConstants {
        int idx;
    };

    TonemapRenderPass::TonemapRenderPass(RenderImageHandle hdrImg, RenderImageHandle finalPrePresent)
        : hdrImg(hdrImg)
        , finalPrePresent(finalPrePresent) {

    }

    RenderPassIO TonemapRenderPass::getIO() {
        RenderPassIO io;

        io.inputs = {
            {
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::PipelineStageFlagBits::eComputeShader,
                vk::AccessFlagBits::eShaderRead,
                hdrImg
            }
        };

        io.outputs = {
            {
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::PipelineStageFlagBits::eComputeShader,
                vk::AccessFlagBits::eShaderWrite,
                finalPrePresent
            }
        };

        return io;
    }

    void TonemapRenderPass::setup(PassSetupCtx& ctx) {
        vku::DescriptorSetLayoutMaker tonemapDslm;
        tonemapDslm.image(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
        tonemapDslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);

        dsl = tonemapDslm.createUnique(ctx.device);

        std::string shaderName = ctx.enableVR ? "tonemap.comp.spv" : "tonemap2d.comp.spv";
        tonemapShader = vku::loadShaderAsset(ctx.device, g_assetDB.addOrGetExisting("Shaders/" + shaderName));

        vku::PipelineLayoutMaker plm;
        plm.descriptorSetLayout(*dsl);
        plm.pushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(TonemapPushConstants));

        pipelineLayout = plm.createUnique(ctx.device);

        vku::ComputePipelineMaker cpm;
        cpm.shader(vk::ShaderStageFlagBits::eCompute, tonemapShader);
        vk::SpecializationMapEntry samplesEntry{ 0, 0, sizeof(int32_t) };
        vk::SpecializationInfo si;
        si.dataSize = sizeof(int32_t);
        si.mapEntryCount = 1;
        si.pMapEntries = &samplesEntry;
        si.pData = &ctx.graphicsSettings.msaaLevel;
        cpm.specializationInfo(si);

        pipeline = cpm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        descriptorSet = dsm.create(ctx.device, ctx.descriptorPool)[0];

        vku::SamplerMaker sm;
        sampler = sm.createUnique(ctx.device);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(descriptorSet);

        dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
        dsu.image(*sampler, ctx.rtResources.at(finalPrePresent).image.imageView(), vk::ImageLayout::eGeneral);

        dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*sampler, ctx.rtResources.at(hdrImg).image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        dsu.update(ctx.device);
    }

    void TonemapRenderPass::execute(RenderCtx& ctx) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        TracyVkZone((*ctx.tracyContexts)[ctx.imageIndex], *ctx.cmdBuf, "Tonemap/Postprocessing");
#endif
        auto& cmdBuf = ctx.cmdBuf;
        //finalPrePresent.setLayout(*cmdBuf, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderWrite);
        vku::transitionLayout(*cmdBuf, ctx.rtResources.at(finalPrePresent).image.image(),
            vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderWrite);

        //::imageBarrier(*cmdBuf, rtResources.at(polyImage).image.image(), vk::ImageLayout::eGeneral, vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader);

        cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, descriptorSet, nullptr);
        cmdBuf->bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
        TonemapPushConstants tpc{ 0 };
        cmdBuf->pushConstants<TonemapPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, tpc);

        cmdBuf->dispatch((ctx.width + 15) / 16, (ctx.height + 15) / 16, 1);

        if (ctx.enableVR) {
            vku::transitionLayout(*cmdBuf, ctx.rtResources.at(finalPrePresentR).image.image(),
                vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral,
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader,
                vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderWrite);

            cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, rDescriptorSet, nullptr);
            TonemapPushConstants tpc{ 1 };
            cmdBuf->pushConstants<TonemapPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, tpc);
            cmdBuf->dispatch((ctx.width + 15) / 16, (ctx.height + 15) / 16, 1);

            /* vku::transitionLayout(*cmdBuf, ctx.rtResources.at(finalPrePresentR).image.image(),
                 vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal,
                 vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
                 vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);*/
        }

        vku::transitionLayout(*cmdBuf, ctx.rtResources.at(finalPrePresent).image.image(),
            vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

        // account for implicit renderpass transition
        //finalPrePresent.setCurrentLayout(vk::ImageLayout::eTransferSrcOptimal);
    }

    void TonemapRenderPass::setRightFinalImage(PassSetupCtx& ctx, RenderImageHandle right) {
        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        rDescriptorSet = dsm.create(ctx.device, ctx.descriptorPool)[0];

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(rDescriptorSet);

        finalPrePresentR = right;

        dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
        dsu.image(*sampler, ctx.rtResources.at(finalPrePresentR).image.imageView(), vk::ImageLayout::eGeneral);

        dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*sampler, ctx.rtResources.at(hdrImg).image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        dsu.update(ctx.device);
    }
}