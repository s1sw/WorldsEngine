#include "RenderPasses.hpp"
#include "../Core/Engine.hpp"
#include "Render.hpp"
#include "ShaderCache.hpp"

namespace worlds {
    struct TonemapPushConstants {
        float aoIntensity;
        int idx;
        float exposureBias;
    };

    static ConVar aoIntensity("r_gtaoIntensity", "1.0");
    static ConVar exposureBias("r_exposure", "0.5");

    TonemapRenderPass::TonemapRenderPass(RenderTexture* hdrImg, RenderTexture* finalPrePresent, RenderTexture* gtaoImg)
        : finalPrePresent(finalPrePresent) 
        , hdrImg(hdrImg)
        , gtaoImg{ gtaoImg } {

    }

    void TonemapRenderPass::setup(PassSetupCtx& psCtx) {
        auto& ctx = psCtx.vkCtx;
        vku::DescriptorSetLayoutMaker tonemapDslm;
        tonemapDslm.image(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
        tonemapDslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);
        tonemapDslm.image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);

        dsl = tonemapDslm.createUnique(ctx.device);
        
        std::string shaderName = "tonemap.comp.spv";//psCtx.enableVR ? "tonemap.comp.spv" : "tonemap2d.comp.spv";
        tonemapShader = ShaderCache::getModule(ctx.device, g_assetDB.addOrGetExisting("Shaders/" + shaderName));

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
        descriptorSet = std::move(dsm.createUnique(ctx.device, ctx.descriptorPool)[0]);

        vku::SamplerMaker sm;
        sampler = sm.createUnique(ctx.device);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(*descriptorSet);

        dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
        dsu.image(*sampler, finalPrePresent->image.imageView(), vk::ImageLayout::eGeneral);

        dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*sampler, hdrImg->image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        dsu.beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*sampler, gtaoImg->image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        dsu.update(ctx.device);
    }

    void TonemapRenderPass::execute(RenderCtx& ctx) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        TracyVkZone((*ctx.tracyContexts)[ctx.imageIndex], *ctx.cmdBuf, "Tonemap/Postprocessing");
#endif
        auto& cmdBuf = ctx.cmdBuf;
        finalPrePresent->image.setLayout(cmdBuf,
            vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderWrite);

        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, *descriptorSet, nullptr);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
        TonemapPushConstants tpc{ aoIntensity.getFloat(), 0, exposureBias.getFloat() };
        cmdBuf.pushConstants<TonemapPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, tpc);

        cmdBuf.dispatch((ctx.width + 15) / 16, (ctx.height + 15) / 16, 1);

        if (ctx.enableVR) {
            finalPrePresentR->image.setLayout(cmdBuf,
                vk::ImageLayout::eGeneral,
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader,
                vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderWrite);

            cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, *rDescriptorSet, nullptr);
            TonemapPushConstants tpc{ aoIntensity.getFloat(), 1, exposureBias.getFloat() };
            cmdBuf.pushConstants<TonemapPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, tpc);
            cmdBuf.dispatch((ctx.width + 15) / 16, (ctx.height + 15) / 16, 1);
        }

        finalPrePresent->image.setLayout(cmdBuf,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
    }

    void TonemapRenderPass::setRightFinalImage(PassSetupCtx& ctx, RenderTexture* right) {
        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        rDescriptorSet = std::move(dsm.createUnique(ctx.vkCtx.device, ctx.vkCtx.descriptorPool)[0]);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(*rDescriptorSet);

        finalPrePresentR = right;

        dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
        dsu.image(*sampler, finalPrePresentR->image.imageView(), vk::ImageLayout::eGeneral);

        dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*sampler, hdrImg->image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        dsu.beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*sampler, gtaoImg->image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        dsu.update(ctx.vkCtx.device);
    }

    TonemapRenderPass::~TonemapRenderPass() {
    }
}
