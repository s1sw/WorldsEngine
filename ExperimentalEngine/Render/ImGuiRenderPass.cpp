#include "RenderPasses.hpp"
#include "../Core/Engine.hpp"
#include "../ImGui/imgui_impl_vulkan.h"
#include "Render.hpp"

namespace worlds {
    ImGuiRenderPass::ImGuiRenderPass(Swapchain& currSwapchain) : currSwapchain(currSwapchain) {

    }

    void ImGuiRenderPass::setup(PassSetupCtx& psCtx) {
        auto& ctx = psCtx.vkCtx;
        vku::RenderpassMaker rPassMaker{};

        rPassMaker.attachmentBegin(currSwapchain.imageFormat());
        rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eLoad);
        rPassMaker.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
        rPassMaker.attachmentInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
        rPassMaker.attachmentFinalLayout(vk::ImageLayout::ePresentSrcKHR);

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
        imguiInit.MinImageCount = psCtx.swapchainImageCount;
        imguiInit.ImageCount = psCtx.swapchainImageCount;
        ImGui_ImplVulkan_Init(&imguiInit, *renderPass);

        vku::executeImmediately(ctx.device, ctx.commandPool, ctx.device.getQueue(ctx.graphicsQueueFamilyIdx, 0), [](vk::CommandBuffer cb) {
            ImGui_ImplVulkan_CreateFontsTexture(cb);
            });
    }

    void ImGuiRenderPass::execute(RenderCtx& ctx, vk::Framebuffer& currFramebuffer) {
        auto& cmdBuf = ctx.cmdBuf;
        ImGui::Render();

        std::array<float, 4> clearColorValue{ 0.0f, 0.0f, 0.0f, 0.0f };
        std::array<vk::ClearValue, 1> clearColours{ vk::ClearValue{clearColorValue} };
        vk::RenderPassBeginInfo rpbi;
        rpbi.renderPass = *renderPass;
        rpbi.framebuffer = currFramebuffer;
        rpbi.renderArea = vk::Rect2D{ {0, 0}, {ctx.width, ctx.height} };
        rpbi.clearValueCount = (uint32_t)clearColours.size();
        rpbi.pClearValues = clearColours.data();
        cmdBuf.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), (VkCommandBuffer)cmdBuf);
        cmdBuf.endRenderPass();
    }

    ImGuiRenderPass::~ImGuiRenderPass() {
        ImGui_ImplVulkan_Shutdown();
    }
}
