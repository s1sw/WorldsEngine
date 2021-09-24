#pragma once
#include "vku.hpp"

namespace vku {
    /// Factory for renderpasses.
    /// example:
    ///     RenderpassMaker rpm;
    ///     rpm.subpassBegin(VK_PIPELINE_BIND_POINT_GRAPHICS);
    ///     rpm.subpassColorAttachment(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    ///
    ///     rpm.attachmentDescription(attachmentDesc);
    ///     rpm.subpassDependency(dependency);
    ///     s.renderPass_ = rpm.create(device);
    class RenderpassMaker {
    public:
        RenderpassMaker() {
        }

        /// Begin an attachment description.
        /// After this you can call attachment* many times
        void attachmentBegin(VkFormat format) {
            VkAttachmentDescription desc{
                .flags = {},
                .format = format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
            s.attachmentDescriptions.push_back(desc);
        }

        void attachmentFlags(VkAttachmentDescriptionFlags value) { s.attachmentDescriptions.back().flags = value; };
        void attachmentFormat(VkFormat value) { s.attachmentDescriptions.back().format = value; };
        void attachmentSamples(VkSampleCountFlagBits value) { s.attachmentDescriptions.back().samples = value; };
        void attachmentLoadOp(VkAttachmentLoadOp value) { s.attachmentDescriptions.back().loadOp = value; };
        void attachmentStoreOp(VkAttachmentStoreOp value) { s.attachmentDescriptions.back().storeOp = value; };
        void attachmentStencilLoadOp(VkAttachmentLoadOp value) { s.attachmentDescriptions.back().stencilLoadOp = value; };
        void attachmentStencilStoreOp(VkAttachmentStoreOp value) { s.attachmentDescriptions.back().stencilStoreOp = value; };
        void attachmentInitialLayout(VkImageLayout value) { s.attachmentDescriptions.back().initialLayout = value; };
        void attachmentFinalLayout(VkImageLayout value) { s.attachmentDescriptions.back().finalLayout = value; };

        /// Start a subpass description.
        /// After this you can can call subpassColorAttachment many times
        /// and subpassDepthStencilAttachment once.
        void subpassBegin(VkPipelineBindPoint bp) {
            VkSubpassDescription desc{};
            desc.pipelineBindPoint = bp;
            s.subpassDescriptions.push_back(desc);
        }

        void subpassColorAttachment(VkImageLayout layout, uint32_t attachment) {
            VkSubpassDescription& subpass = s.subpassDescriptions.back();
            auto* p = getAttachmentReference();
            p->layout = layout;
            p->attachment = attachment;
            if (subpass.colorAttachmentCount == 0) {
                subpass.pColorAttachments = p;
            }
            subpass.colorAttachmentCount++;
        }

        void subpassDepthStencilAttachment(VkImageLayout layout, uint32_t attachment) {
            VkSubpassDescription& subpass = s.subpassDescriptions.back();
            auto* p = getAttachmentReference();
            p->layout = layout;
            p->attachment = attachment;
            subpass.pDepthStencilAttachment = p;
        }

        VkRenderPass create(VkDevice device) const {
            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = (uint32_t)s.attachmentDescriptions.size();
            renderPassInfo.pAttachments = s.attachmentDescriptions.data();
            renderPassInfo.subpassCount = (uint32_t)s.subpassDescriptions.size();
            renderPassInfo.pSubpasses = s.subpassDescriptions.data();
            renderPassInfo.dependencyCount = (uint32_t)s.subpassDependencies.size();
            renderPassInfo.pDependencies = s.subpassDependencies.data();
            renderPassInfo.pNext = s.pNext;

            VkRenderPass pass;
            VKCHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &pass));

            return pass;
        }

        void dependencyBegin(uint32_t srcSubpass, uint32_t dstSubpass) {
            VkSubpassDependency desc{};
            desc.srcSubpass = srcSubpass;
            desc.dstSubpass = dstSubpass;
            s.subpassDependencies.push_back(desc);
        }

        void dependencySrcSubpass(uint32_t value) { s.subpassDependencies.back().srcSubpass = value; };
        void dependencyDstSubpass(uint32_t value) { s.subpassDependencies.back().dstSubpass = value; };
        void dependencySrcStageMask(VkPipelineStageFlags value) { s.subpassDependencies.back().srcStageMask = value; };
        void dependencyDstStageMask(VkPipelineStageFlags value) { s.subpassDependencies.back().dstStageMask = value; };
        void dependencySrcAccessMask(VkAccessFlags value) { s.subpassDependencies.back().srcAccessMask = value; };
        void dependencyDstAccessMask(VkAccessFlags value) { s.subpassDependencies.back().dstAccessMask = value; };
        void dependencyDependencyFlags(VkDependencyFlags value) { s.subpassDependencies.back().dependencyFlags = value; };
        void setPNext(void* pn) { s.pNext = pn; }
    private:
        constexpr static int max_refs = 64;

        VkAttachmentReference* getAttachmentReference() {
            return (s.num_refs < max_refs) ? &s.attachmentReferences[s.num_refs++] : nullptr;
        }

        struct State {
            std::vector<VkAttachmentDescription> attachmentDescriptions;
            std::vector<VkSubpassDescription> subpassDescriptions;
            std::vector<VkSubpassDependency> subpassDependencies;
            std::array<VkAttachmentReference, max_refs> attachmentReferences;
            void* pNext = nullptr;
            int num_refs = 0;
            bool ok_ = false;
        };

        State s;
    };
}
