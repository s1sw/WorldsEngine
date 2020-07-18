#include "RenderPasses.hpp"
#include "Engine.hpp"
#include "imgui_impl_vulkan.h"

ImGuiRenderPass::ImGuiRenderPass(RenderImageHandle imguiTarget) : target(imguiTarget) {

}

RenderPassIO ImGuiRenderPass::getIO() {
    RenderPassIO io;
    io.outputs = {
        {
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eColorAttachmentWrite,
            target
        }
    };

    return io;
}

void ImGuiRenderPass::setup(PassSetupCtx& ctx, RenderCtx& rCtx) {
    vku::RenderpassMaker rPassMaker{};

    rPassMaker.attachmentBegin(vk::Format::eR8G8B8A8Unorm);
    rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eDontCare);
    rPassMaker.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
    rPassMaker.attachmentFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    rPassMaker.attachmentInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);

    rPassMaker.subpassBegin(vk::PipelineBindPoint::eGraphics);
    rPassMaker.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);

    rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
    rPassMaker.dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rPassMaker.dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rPassMaker.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

    renderPass = rPassMaker.createUnique(ctx.device);

    vk::FramebufferCreateInfo fci;
    vk::ImageView finalImageView = rCtx.rtResources.at(target).image.imageView();
    fci.attachmentCount = 1;
    fci.pAttachments = &finalImageView;
    fci.renderPass = *renderPass;
    fci.layers = 1;
    fb = ctx.device.createFramebufferUnique(fci);
}

void ImGuiRenderPass::execute(RenderCtx& ctx) {
    auto& cmdBuf = ctx.cmdBuf;

    std::array<float, 4> clearColorValue{ 0.0f, 0.0f, 0.0f, 1 };
    std::array<vk::ClearValue, 1> clearColours{ vk::ClearValue{clearColorValue} };
    vk::RenderPassBeginInfo rpbi;
    rpbi.renderPass = *renderPass;
    rpbi.framebuffer = *fb;
    rpbi.renderArea = vk::Rect2D{ {0, 0}, {ctx.width, ctx.height} };
    rpbi.clearValueCount = clearColours.size();
    rpbi.pClearValues = clearColours.data();
    cmdBuf->beginRenderPass(rpbi, vk::SubpassContents::eInline);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmdBuf);
    cmdBuf->endRenderPass();
}