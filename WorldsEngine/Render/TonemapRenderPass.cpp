#include "RenderPasses.hpp"
#include "../Core/Engine.hpp"
#include "Render.hpp"
#include "ShaderCache.hpp"

namespace worlds {
    struct TonemapPushConstants {
        int idx;
        float exposureBias;
    };

    static ConVar exposureBias("r_exposure", "0.5");

    TonemapRenderPass::TonemapRenderPass(
            VulkanHandles* handles,
            RenderTexture* hdrImg,
            RenderTexture* finalPrePresent)
        : finalPrePresent{finalPrePresent}
        , hdrImg {hdrImg}
        , handles {handles} {

    }

    void TonemapRenderPass::setup(RenderContext& ctx, vk::DescriptorPool descriptorPool) {
        dsPool = descriptorPool;
        vku::DescriptorSetLayoutMaker tonemapDslm;
        tonemapDslm.image(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
        tonemapDslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);

        dsl = tonemapDslm.createUnique(handles->device);

        auto msaaSamples = hdrImg->image.info().samples;
        std::string shaderName = (int)msaaSamples > 1 ? "tonemap.comp.spv" : "tonemap_nomsaa.comp.spv";
        tonemapShader = ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/" + shaderName));

        vku::PipelineLayoutMaker plm;
        plm.descriptorSetLayout(*dsl);
        plm.pushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(TonemapPushConstants));

        pipelineLayout = plm.createUnique(handles->device);

        vku::ComputePipelineMaker cpm;
        cpm.shader(vk::ShaderStageFlagBits::eCompute, tonemapShader);
        vk::SpecializationMapEntry samplesEntry{ 0, 0, sizeof(int32_t) };
        vk::SpecializationInfo si;
        si.dataSize = sizeof(msaaSamples);
        si.mapEntryCount = 1;
        si.pMapEntries = &samplesEntry;
        si.pData = &msaaSamples;
        cpm.specializationInfo(si);

        pipeline = cpm.createUnique(handles->device, handles->pipelineCache, *pipelineLayout);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        descriptorSet = std::move(dsm.createUnique(handles->device, descriptorPool)[0]);

        vku::SamplerMaker sm;
        sampler = sm.createUnique(handles->device);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(*descriptorSet);

        dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
        dsu.image(*sampler, finalPrePresent->image.imageView(), vk::ImageLayout::eGeneral);

        dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*sampler, hdrImg->image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        dsu.update(handles->device);
    }

    void TonemapRenderPass::execute(RenderContext& ctx) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        TracyVkZone((*ctx.tracyContexts)[ctx.imageIndex], *ctx.cmdBuf, "Tonemap/Postprocessing");
#endif
        auto& cmdBuf = ctx.cmdBuf;
        finalPrePresent->image.setLayout(cmdBuf,
            vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderWrite);

        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, *descriptorSet, nullptr);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
        TonemapPushConstants tpc{ 0, exposureBias.getFloat() };
        cmdBuf.pushConstants<TonemapPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, tpc);

        cmdBuf.dispatch((ctx.passWidth + 15) / 16, (ctx.passHeight + 15) / 16, 1);

        if (ctx.passSettings.enableVR) {
            finalPrePresentR->image.setLayout(cmdBuf,
                vk::ImageLayout::eGeneral,
                vk::PipelineStageFlagBits::eComputeShader,
                vk::AccessFlagBits::eShaderWrite);

            cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, *rDescriptorSet, nullptr);
            TonemapPushConstants tpc{ 1, exposureBias.getFloat() };
            cmdBuf.pushConstants<TonemapPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, tpc);
            cmdBuf.dispatch((ctx.passWidth + 15) / 16, (ctx.passHeight + 15) / 16, 1);
        }

        finalPrePresent->image.setLayout(cmdBuf,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
    }

    void TonemapRenderPass::setRightFinalImage(RenderTexture* right) {
        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        rDescriptorSet = std::move(dsm.createUnique(handles->device, dsPool)[0]);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(*rDescriptorSet);

        finalPrePresentR = right;

        dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
        dsu.image(*sampler, finalPrePresentR->image.imageView(), vk::ImageLayout::eGeneral);

        dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*sampler, hdrImg->image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        dsu.update(handles->device);
    }

    TonemapRenderPass::~TonemapRenderPass() {
    }
}
