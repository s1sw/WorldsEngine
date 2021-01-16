#include "RenderPasses.hpp"
#include "Render.hpp"
#include "ShaderCache.hpp"

namespace worlds {
    struct BlurPushConstants {
        glm::vec2 direction;
        uint32_t idx;
    };

    BlurRenderPass::BlurRenderPass(VKRenderer* renderer, RenderTexture* src)
        : src{ src }
        , renderer{ renderer }
        , numLayers{ src->image.info().arrayLayers }{

    }

    void BlurRenderPass::setup(PassSetupCtx& ctx) {
        worlds::RTResourceCreateInfo rtrci;
        rtrci.aspectFlags = vk::ImageAspectFlagBits::eColor;
        rtrci.viewType = vk::ImageViewType::e2DArray;

        vk::ImageCreateInfo ici;
        ici.imageType = vk::ImageType::e2D;
        ici.extent = vk::Extent3D{ ctx.passWidth, ctx.passHeight, 1 };
        ici.arrayLayers = numLayers;
        ici.mipLevels = 1;
        ici.format = vk::Format::eR8G8B8A8Unorm;
        ici.initialLayout = vk::ImageLayout::eUndefined;
        ici.samples = vk::SampleCountFlagBits::e1;
        ici.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
        rtrci.ici = ici;
        tmpTarget = renderer->createRTResource(rtrci, "blur tmp");

        shader = ShaderCache::getModule(ctx.vkCtx.device, g_assetDB.addOrGetExisting("Shaders/blur.comp.spv"));

        vku::SamplerMaker sm;
        sm.magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear);
        samp = sm.createUnique(ctx.vkCtx.device);

        vku::DescriptorSetLayoutMaker dslm;
        dslm.image(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
        dslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);
        dsl = dslm.createUnique(ctx.vkCtx.device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(BlurPushConstants));
        plm.descriptorSetLayout(*dsl);
        pipelineLayout = plm.createUnique(ctx.vkCtx.device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        dsm.layout(*dsl);
        dses = dsm.createUnique(ctx.vkCtx.device, ctx.vkCtx.descriptorPool);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(*dses[0]);
        dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
        dsu.image(*samp, tmpTarget->image.imageView(), vk::ImageLayout::eGeneral);
        dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*samp, src->image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        dsu.beginDescriptorSet(*dses[1]);
        dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
        dsu.image(*samp, src->image.imageView(), vk::ImageLayout::eGeneral);
        dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*samp, tmpTarget->image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        assert(dsu.ok());
        dsu.update(ctx.vkCtx.device);

        vku::ComputePipelineMaker cpm;
        cpm.shader(vk::ShaderStageFlagBits::eCompute, shader);
        pipeline = cpm.createUnique(ctx.vkCtx.device, ctx.vkCtx.pipelineCache, *pipelineLayout);
    }

    void BlurRenderPass::execute(RenderCtx& ctx) {
        tmpTarget->image.setLayout(*ctx.cmdBuf, vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite);

        ctx.cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, *dses[0], nullptr);
        ctx.cmdBuf->bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);

        BlurPushConstants pc;
        pc.direction = glm::vec2(1.0f, 0.0f);
        ctx.cmdBuf->pushConstants<BlurPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, pc);
        ctx.cmdBuf->dispatch((ctx.width + 15) / 16, (ctx.height + 15) / 16, numLayers);

        tmpTarget->image.setLayout(*ctx.cmdBuf, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        src->image.setLayout(*ctx.cmdBuf, vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite);

        ctx.cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, *dses[1], nullptr);
        pc.direction = glm::vec2(0.0f, 1.0f);
        ctx.cmdBuf->pushConstants<BlurPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, pc);
        ctx.cmdBuf->dispatch((ctx.width + 15) / 16, (ctx.height + 15) / 16, numLayers);

        src->image.setLayout(*ctx.cmdBuf, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
    }

    BlurRenderPass::~BlurRenderPass() {
        delete tmpTarget;
    }
}