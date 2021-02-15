#include "../Core/Engine.hpp"
#include "Render.hpp"

namespace worlds {
    const int BRDF_LUT_RES = 256;

    BRDFLUTRenderer::BRDFLUTRenderer(VulkanHandles& ctx) {
        vku::RenderpassMaker rpm;
        rpm.attachmentBegin(vk::Format::eR16G16Sfloat);
        rpm.attachmentLoadOp(vk::AttachmentLoadOp::eDontCare);
        rpm.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
        rpm.attachmentFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        rpm.subpassBegin(vk::PipelineBindPoint::eGraphics);
        rpm.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);

        rpm.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
        rpm.dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        rpm.dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        rpm.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

        renderPass = rpm.createUnique(ctx.device);

        vku::PipelineLayoutMaker plm;
        pipelineLayout = plm.createUnique(ctx.device);

        AssetID vsId = g_assetDB.addOrGetExisting("Shaders/full_tri.vert.spv");
        AssetID fsId = g_assetDB.addOrGetExisting("Shaders/brdf_lut.frag.spv");

        fs = vku::loadShaderAsset(ctx.device, fsId);
        vs = vku::loadShaderAsset(ctx.device, vsId);

        vku::PipelineMaker pm{ BRDF_LUT_RES, BRDF_LUT_RES };
        pm.shader(vk::ShaderStageFlagBits::eFragment, fs);
        pm.shader(vk::ShaderStageFlagBits::eVertex, vs);

        pm.cullMode(vk::CullModeFlagBits::eNone);
        pipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout, *renderPass);
    }

    void BRDFLUTRenderer::render(VulkanHandles& ctx, vku::GenericImage& target) {
        vk::FramebufferCreateInfo fci;
        fci.attachmentCount = 1;
        vk::ImageView targetView = target.imageView();
        fci.pAttachments = &targetView;
        fci.width = target.extent().width;
        fci.height = target.extent().height;
        fci.renderPass = *renderPass;
        fci.layers = 1;

        auto fb = ctx.device.createFramebufferUnique(fci);

        vku::executeImmediately(ctx.device, ctx.commandPool, ctx.device.getQueue(ctx.graphicsQueueFamilyIdx, 0), [&](vk::CommandBuffer cb) {
            vk::RenderPassBeginInfo rpbi;
            rpbi.renderPass = *renderPass;
            rpbi.framebuffer = *fb;
            rpbi.renderArea = vk::Rect2D{ {0, 0}, {BRDF_LUT_RES, BRDF_LUT_RES} };
            rpbi.clearValueCount = 0;
            rpbi.pClearValues = nullptr;

            cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
            cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
            cb.draw(3, 1, 0, 0);
            cb.endRenderPass();
            });

        fb.reset();
    }
}
