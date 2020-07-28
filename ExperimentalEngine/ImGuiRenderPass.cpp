#include "RenderPasses.hpp"
#include "Engine.hpp"
#include "imgui_impl_vulkan.h"

ImGuiRenderPass::ImGuiRenderPass(RenderImageHandle imguiTarget) : target(imguiTarget) {

}

RenderPassIO ImGuiRenderPass::getIO() {
    RenderPassIO io;
    io.outputs = {
        {
            vk::ImageLayout::eTransferSrcOptimal,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eColorAttachmentWrite,
            target
        }
    };

    return io;
}

void ImGuiRenderPass::setup(PassSetupCtx& ctx) {
    vku::RenderpassMaker rPassMaker{};

    rPassMaker.attachmentBegin(vk::Format::eR8G8B8A8Unorm);
    rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eLoad);
    rPassMaker.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
    rPassMaker.attachmentInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
    rPassMaker.attachmentFinalLayout(vk::ImageLayout::eTransferSrcOptimal);

    rPassMaker.subpassBegin(vk::PipelineBindPoint::eGraphics);
    rPassMaker.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);

    rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
    rPassMaker.dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rPassMaker.dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rPassMaker.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

    renderPass = rPassMaker.createUnique(ctx.device);

    ImGui_ImplVulkan_InitInfo imguiInit;
    memset(&imguiInit, 0, sizeof(imguiInit));
    imguiInit.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    imguiInit.Device = ctx.device;
    imguiInit.Instance = ctx.instance;
    imguiInit.DescriptorPool = ctx.descriptorPool;
    imguiInit.PhysicalDevice = ctx.physicalDevice;
    imguiInit.PipelineCache = ctx.pipelineCache;
    imguiInit.Queue = ctx.device.getQueue(ctx.graphicsQueueFamilyIdx, 0);
    imguiInit.QueueFamily = ctx.graphicsQueueFamilyIdx;
    imguiInit.MinImageCount = ctx.swapchainImageCount;
    imguiInit.ImageCount = ctx.swapchainImageCount;
    ImGui_ImplVulkan_Init(&imguiInit, *renderPass);

    vku::executeImmediately(ctx.device, ctx.commandPool, ctx.device.getQueue(ctx.graphicsQueueFamilyIdx, 0), [](vk::CommandBuffer cb) {
        ImGui_ImplVulkan_CreateFontsTexture(cb);
        });

    vk::FramebufferCreateInfo fci;
    vk::ImageView finalImageView = ctx.rtResources.at(target).image.imageView();
    fci.attachmentCount = 1;
    fci.pAttachments = &finalImageView;
    fci.renderPass = *renderPass;
    fci.layers = 1;
    auto extent = ctx.rtResources.at(target).image.info().extent;
    fci.width = extent.width;
    fci.height = extent.height;
    fb = ctx.device.createFramebufferUnique(fci);
}

void ImGuiRenderPass::execute(RenderCtx& ctx) {
    auto& cmdBuf = ctx.cmdBuf;

    std::array<float, 4> clearColorValue{ 0.0f, 0.0f, 0.0f, 0.0f };
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