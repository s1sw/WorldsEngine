#include "RenderPasses.hpp"
#include "../Core/Engine.hpp"
#include "../ImGui/imgui_impl_vulkan.h"
#include <ImGui/imgui_internal.h>
#include "Render.hpp"
#include "vku/RenderpassMaker.hpp"

namespace worlds {
    ImGuiRenderPass::ImGuiRenderPass(VulkanHandles* handles, Swapchain& currSwapchain)
        : currSwapchain {currSwapchain}
        , handles {handles} {

    }

    void ImGuiRenderPass::setup() {
        vku::RenderpassMaker rPassMaker{};

        rPassMaker.attachmentBegin(currSwapchain.imageFormat());
        rPassMaker.attachmentLoadOp(VK_ATTACHMENT_LOAD_OP_LOAD);
        rPassMaker.attachmentStoreOp(VK_ATTACHMENT_STORE_OP_STORE);
        rPassMaker.attachmentInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        rPassMaker.attachmentFinalLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        rPassMaker.subpassBegin(VK_PIPELINE_BIND_POINT_GRAPHICS);
        rPassMaker.subpassColorAttachment(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0);

        rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
        rPassMaker.dependencySrcStageMask(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        rPassMaker.dependencyDstStageMask(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        rPassMaker.dependencyDstAccessMask(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

        renderPass = rPassMaker.create(handles->device);

        ImGui_ImplVulkan_InitInfo imguiInit;
        memset(&imguiInit, 0, sizeof(imguiInit));
        imguiInit.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        imguiInit.Device = handles->device;
        imguiInit.Instance = handles->instance;
        imguiInit.DescriptorPool = handles->descriptorPool;
        imguiInit.PhysicalDevice = handles->physicalDevice;
        imguiInit.PipelineCache = handles->pipelineCache;
        vkGetDeviceQueue(handles->device, handles->graphicsQueueFamilyIdx, 0, &imguiInit.Queue);
        imguiInit.QueueFamily = handles->graphicsQueueFamilyIdx;
        imguiInit.MinImageCount = currSwapchain.images.size();
        imguiInit.ImageCount = currSwapchain.images.size();
        ImGui_ImplVulkan_Init(&imguiInit, renderPass);

        auto queue = imguiInit.Queue;

        vku::executeImmediately(handles->device, handles->commandPool, queue,
            [](VkCommandBuffer cb) {
            ImGui_ImplVulkan_CreateFontsTexture(cb);
        });
    }

    void ImGuiRenderPass::execute(VkCommandBuffer& cmdBuf,
            uint32_t width, uint32_t height,
            VkFramebuffer currFramebuffer, ImDrawData* drawData) {
        VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        label.pLabelName = "Dear ImGui Pass";
        label.color[0] = 0.082f;
        label.color[1] = 0.086f;
        label.color[2] = 0.090f;
        label.color[3] = 1.0f;
        vkCmdBeginDebugUtilsLabelEXT(cmdBuf, &label);

        VkClearValue clearVal;
        clearVal.color = VkClearColorValue{ 0.0f, 0.0f, 0.0f, 0.0f };
        VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpbi.renderPass = renderPass;
        rpbi.framebuffer = currFramebuffer;
        rpbi.renderArea = VkRect2D{ {0, 0}, {width, height} };
        rpbi.clearValueCount = 1;
        rpbi.pClearValues = &clearVal;
        vkCmdBeginRenderPass(cmdBuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(drawData, (VkCommandBuffer)cmdBuf);
        vkCmdEndRenderPass(cmdBuf);
        vkCmdEndDebugUtilsLabelEXT(cmdBuf);
    }

    ImGuiRenderPass::~ImGuiRenderPass() {
        ImGui_ImplVulkan_Shutdown();
    }
}
