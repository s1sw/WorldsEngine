#include "RenderPasses.hpp"
#include "Engine.hpp"

TonemapRenderPass::TonemapRenderPass(RenderImageHandle hdrImg, RenderImageHandle imguiImg, RenderImageHandle finalPrePresent)
    : hdrImg(hdrImg)
    , finalPrePresent(finalPrePresent)
    , imguiImg(imguiImg) {

}

RenderPassIO TonemapRenderPass::getIO() {
    RenderPassIO io;

    io.inputs = {
        {
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderRead,
            hdrImg
        },
        {
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderRead,
            imguiImg
        }
    };

    io.outputs = {
        {
            vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderWrite,
            finalPrePresent
        }
    };

    return io;
}

void TonemapRenderPass::setup(PassSetupCtx& ctx, RenderCtx& rCtx) {
    vku::DescriptorSetLayoutMaker tonemapDslm;
    tonemapDslm.image(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
    tonemapDslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);
    tonemapDslm.image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);

    dsl = tonemapDslm.createUnique(ctx.device);

    tonemapShader = vku::loadShaderAsset(ctx.device, g_assetDB.addAsset("Shaders/tonemap.comp.spv"));

    vku::PipelineLayoutMaker plm;
    plm.descriptorSetLayout(*dsl);

    pipelineLayout = plm.createUnique(ctx.device);

    vku::ComputePipelineMaker cpm;
    cpm.shader(vk::ShaderStageFlagBits::eCompute, tonemapShader);
    vk::SpecializationMapEntry samplesEntry{ 0, 0, sizeof(int32_t) };
    vk::SpecializationInfo si;
    si.dataSize = sizeof(int32_t);
    si.mapEntryCount = 1;
    si.pMapEntries = &samplesEntry;
    si.pData = &ctx.graphicsSettings.msaaLevel;
    pipeline = cpm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout);

    vku::DescriptorSetMaker dsm;
    dsm.layout(*dsl);
    descriptorSet = dsm.create(ctx.device, ctx.descriptorPool)[0];

    vku::SamplerMaker sm;
    sampler = sm.createUnique(ctx.device);

    vku::DescriptorSetUpdater dsu;
    dsu.beginDescriptorSet(descriptorSet);

    dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
    dsu.image(*sampler, rCtx.rtResources.at(finalPrePresent).image.imageView(), vk::ImageLayout::eGeneral);

    dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
    dsu.image(*sampler, rCtx.rtResources.at(hdrImg).image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    dsu.beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler);
    dsu.image(*sampler, rCtx.rtResources.at(imguiImg).image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    dsu.update(ctx.device);
}

void TonemapRenderPass::execute(RenderCtx& ctx) {
#ifdef TRACY_ENABLE
    ZoneScoped;
    TracyVkZone(tracyContexts[ctx.imageIndex], *ctx.cmdBuf, "Tonemap/Postprocessing");
#endif
    auto& cmdBuf = ctx.cmdBuf;
    //finalPrePresent.setLayout(*cmdBuf, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderWrite);

    //::imageBarrier(*cmdBuf, rtResources.at(polyImage).image.image(), vk::ImageLayout::eGeneral, vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader);

    cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, descriptorSet, nullptr);
    cmdBuf->bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);

    cmdBuf->dispatch((ctx.width + 15) / 16, (ctx.height + 15) / 16, 1);

   // vku::transitionLayout(*cmdBuf, finalPrePresent.image(),
   //     vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal,
   //     vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
   //     vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead);

    

    // account for implicit renderpass transition
    //finalPrePresent.setCurrentLayout(vk::ImageLayout::eTransferSrcOptimal);
}