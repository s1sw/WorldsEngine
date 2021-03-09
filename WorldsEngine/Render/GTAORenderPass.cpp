#include "RenderPasses.hpp"
#include "Render.hpp"
#include "ShaderCache.hpp"

namespace worlds {
    struct GTAOPushConstants {
        float aspect;
        float angleOffset;
        float spatialOffset;
        float radius;
        glm::vec2 viewSizeRcp;
        glm::vec2 viewSize;
        float limit;
        float falloff;
        float thicknessMix;
        glm::mat4 proj;
    };

    GTAORenderPass::GTAORenderPass(VKRenderer* renderer, RenderTexture* depth, RenderTexture* out)
        : depth{ depth }
        , out{ out }
        , renderer{ renderer }
        , frameIdx{ 0 } {

    }

    void GTAORenderPass::setup(PassSetupCtx& ctx) {
        brp = new BlurRenderPass(renderer, out);
        brp->setup(ctx);

        shader = ShaderCache::getModule(ctx.vkCtx.device, g_assetDB.addOrGetExisting("Shaders/gtao.comp.spv"));

        vku::SamplerMaker sm;
        sm.magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear);
        samp = sm.createUnique(ctx.vkCtx.device);

        vku::DescriptorSetLayoutMaker dslm;
        dslm.image(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
        dslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);
        dsl = dslm.createUnique(ctx.vkCtx.device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(GTAOPushConstants));
        plm.descriptorSetLayout(*dsl);
        pipelineLayout = plm.createUnique(ctx.vkCtx.device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        descriptorSet = std::move(dsm.createUnique(ctx.vkCtx.device, ctx.vkCtx.descriptorPool)[0]);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(*descriptorSet);
        dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
        dsu.image(*samp, out->image.imageView(), vk::ImageLayout::eGeneral);
        dsu.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
        dsu.image(*samp, depth->image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        assert(dsu.ok());
        dsu.update(ctx.vkCtx.device);

        vku::ComputePipelineMaker cpm;
        cpm.shader(vk::ShaderStageFlagBits::eCompute, shader);
        pipeline = cpm.createUnique(ctx.vkCtx.device, ctx.vkCtx.pipelineCache, *pipelineLayout);

        vku::executeImmediately(ctx.vkCtx.device, ctx.vkCtx.commandPool, ctx.vkCtx.device.getQueue(ctx.vkCtx.graphicsQueueFamilyIdx, 0), [&](auto cb) {
            out->image.setLayout(cb, vk::ImageLayout::eGeneral);
            });
    }

    static ConVar gtaoLimit{ "r_gtaoLimit", "100" };
    static ConVar gtaoRadius{ "r_gtaoRadius", "2.5" };
    static ConVar gtaoFalloff{ "r_gtaoFalloff", "3" };
    static ConVar gtaoThicknessMix{ "r_gtaoThicknessMix", "0.2" };
    static ConVar gtaoBlur{ "r_gtaoBlur", "1" };

    void GTAORenderPass::execute(RenderCtx& ctx) {
        depth->image.setLayout(ctx.cmdBuf, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::AccessFlagBits::eShaderRead,
            vk::ImageAspectFlagBits::eDepth);

        ctx.cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, *descriptorSet, nullptr);
        ctx.cmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);

        out->image.setLayout(ctx.cmdBuf, vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite);

        static GTAOPushConstants pc;
        pc.aspect = (float)ctx.width / (float)ctx.height;
        pc.viewSizeRcp = glm::vec2(1.0f / (float)ctx.width, 1.0f / (float)ctx.height);
        pc.viewSize = glm::vec2(ctx.width, ctx.height);
        pc.proj = glm::inverse(ctx.cam->getProjectionMatrixZO((float)ctx.width / (float)ctx.height));

        pc.limit = gtaoLimit.getFloat();
        pc.radius = gtaoRadius.getFloat();
        pc.falloff = gtaoFalloff.getFloat();
        pc.thicknessMix = gtaoThicknessMix.getFloat();

        ctx.cmdBuf.pushConstants<GTAOPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, pc);
        ctx.cmdBuf.dispatch((ctx.width + 15) / 16, (ctx.height + 15) / 16, ctx.enableVR + 1);

        out->image.setLayout(ctx.cmdBuf, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

        if (gtaoBlur.getInt())
            brp->execute(ctx);

        frameIdx++;
        frameIdx %= 2;
    }

    GTAORenderPass::~GTAORenderPass() {
        delete brp;
    }
}
