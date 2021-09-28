#include "../Core/Engine.hpp"
#include "RenderInternal.hpp"
#include "vku/RenderpassMaker.hpp"

namespace worlds {
    const int BRDF_LUT_RES = 256;

    BRDFLUTRenderer::BRDFLUTRenderer(VulkanHandles& ctx) {
        vku::RenderpassMaker rpm;
        rpm.attachmentBegin(VK_FORMAT_R16G16_SFLOAT);
        rpm.attachmentLoadOp(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
        rpm.attachmentStoreOp(VK_ATTACHMENT_STORE_OP_STORE);
        rpm.attachmentFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        rpm.subpassBegin(VK_PIPELINE_BIND_POINT_GRAPHICS);
        rpm.subpassColorAttachment(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0);

        rpm.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
        rpm.dependencySrcStageMask(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        rpm.dependencyDstStageMask(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        rpm.dependencyDstAccessMask(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

        renderPass = rpm.create(ctx.device);

        vku::PipelineLayoutMaker plm;
        pipelineLayout = plm.create(ctx.device);

        AssetID vsId = AssetDB::pathToId("Shaders/full_tri.vert.spv");
        AssetID fsId = AssetDB::pathToId("Shaders/brdf_lut.frag.spv");

        fs = vku::loadShaderAsset(ctx.device, fsId);
        vs = vku::loadShaderAsset(ctx.device, vsId);

        vku::PipelineMaker pm{ BRDF_LUT_RES, BRDF_LUT_RES };
        pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, fs);
        pm.shader(VK_SHADER_STAGE_VERTEX_BIT, vs);

        pm.cullMode(VK_CULL_MODE_NONE);
        pipeline = pm.create(ctx.device, ctx.pipelineCache, pipelineLayout, renderPass);
    }

    void BRDFLUTRenderer::render(VulkanHandles& ctx, vku::GenericImage& target) {
        VkFramebufferCreateInfo fci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fci.attachmentCount = 1;
        VkImageView targetView = target.imageView();
        fci.pAttachments = &targetView;
        fci.width = target.extent().width;
        fci.height = target.extent().height;
        fci.renderPass = renderPass;
        fci.layers = 1;

        VkFramebuffer fb;
        VKCHECK(vkCreateFramebuffer(ctx.device, &fci, nullptr, &fb));

        VkQueue queue;
        vkGetDeviceQueue(ctx.device, ctx.graphicsQueueFamilyIdx, 0, &queue);

        vku::executeImmediately(ctx.device, ctx.commandPool, queue, [&](VkCommandBuffer cb) {
            VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            rpbi.renderPass = renderPass;
            rpbi.framebuffer = fb;
            rpbi.renderArea = VkRect2D{ {0, 0}, {BRDF_LUT_RES, BRDF_LUT_RES} };
            rpbi.clearValueCount = 0;
            rpbi.pClearValues = nullptr;

            vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdDraw(cb, 3, 1, 0, 0);
            vkCmdEndRenderPass(cb);
            });

        vkDestroyFramebuffer(ctx.device, fb, nullptr);
    }
}
