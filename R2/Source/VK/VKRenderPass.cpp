#include <R2/VKRenderPass.hpp>
#include <R2/VKTexture.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <volk.h>
#include <malloc.h>

namespace R2::VK
{
    VkAttachmentStoreOp convertStoreOp(StoreOp op)
    {
        switch (op)
        {
        case StoreOp::Store:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case StoreOp::DontCare:
        default:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
    }

    VkAttachmentLoadOp convertLoadOp(LoadOp op)
    {
        switch (op)
        {
        case LoadOp::Load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadOp::Clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOp::DontCare:
        default:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
    }

    RenderPass::RenderPass()
        : numColorAttachments(0)
    {
        depthAttachment.Texture = nullptr;
    }

    RenderPass& RenderPass::RenderArea(uint32_t width, uint32_t height)
    {
        this->width = width;
        this->height = height;

        return *this;
    }

    RenderPass& RenderPass::ColorAttachment(Texture* tex, LoadOp loadOp, StoreOp storeOp)
    {
        AttachmentInfo ai{};
        ai.Texture = tex;
        ai.LoadOp = loadOp;
        ai.StoreOp = storeOp;

        colorAttachments[numColorAttachments] = ai;
        numColorAttachments++;

        return *this;
    }

    RenderPass& RenderPass::ColorAttachmentClearValue(ClearValue cv)
    {
        colorAttachments[numColorAttachments - 1].ClearValue = cv;

        return *this;
    }

    RenderPass& RenderPass::DepthAttachment(Texture* tex, LoadOp loadOp, StoreOp storeOp)
    {
        AttachmentInfo ai{};
        ai.Texture = tex;
        ai.LoadOp = loadOp;
        ai.StoreOp = storeOp;
        depthAttachment = ai;

        return *this;
    }

    RenderPass& RenderPass::DepthAttachmentClearValue(ClearValue cv)
    {
        depthAttachment.ClearValue = cv;

        return *this;
    }

    void RenderPass::Begin(CommandBuffer cb)
    {
        for (int i = 0; i < numColorAttachments; i++)
        {
            colorAttachments[i].Texture->Acquire(cb, ImageLayout::AttachmentOptimal, AccessFlags::ColorAttachmentReadWrite);
        }

        if (depthAttachment.Texture)
            depthAttachment.Texture->Acquire(cb, ImageLayout::AttachmentOptimal, AccessFlags::DepthStencilAttachmentReadWrite);

        VkRenderingInfo renderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderInfo.renderArea = VkRect2D{ { 0, 0 }, { width, height }, };
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = numColorAttachments;

        VkRenderingAttachmentInfo depthAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        if (depthAttachment.Texture)
        {
            depthAttachmentInfo.clearValue.depthStencil.depth = depthAttachment.ClearValue.DepthStencil.Depth;
            depthAttachmentInfo.imageView = depthAttachment.Texture->GetView();
            depthAttachmentInfo.storeOp = convertStoreOp(depthAttachment.StoreOp);
            depthAttachmentInfo.loadOp = convertLoadOp(depthAttachment.LoadOp);
            depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            renderInfo.pDepthAttachment = &depthAttachmentInfo;
        }

        // Is alloca the right choice here? We certainly don't want to heap allocate.
        VkRenderingAttachmentInfo* colorAttachmentInfos = 
            static_cast<VkRenderingAttachmentInfo*>(alloca(sizeof(VkRenderingAttachmentInfo) * numColorAttachments));

        for (int i = 0; i < numColorAttachments; i++)
        {
            const AttachmentInfo& colorAttachment = colorAttachments[i];
            VkRenderingAttachmentInfo& colorAttachmentInfo = colorAttachmentInfos[i];
            colorAttachmentInfo = VkRenderingAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

            for (int j = 0; j < 4; j++)
                colorAttachmentInfo.clearValue.color.uint32[j] = colorAttachment.ClearValue.Color.Uint32[j];

            colorAttachmentInfo.imageView = colorAttachment.Texture->GetView();
            colorAttachmentInfo.storeOp = convertStoreOp(colorAttachment.StoreOp);
            colorAttachmentInfo.loadOp = convertLoadOp(colorAttachment.LoadOp);
            colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        }

        renderInfo.pColorAttachments = colorAttachmentInfos;

        vkCmdBeginRendering(cb.GetNativeHandle(), &renderInfo);
    }

    void RenderPass::End(CommandBuffer cb)
    {
        vkCmdEndRendering(cb.GetNativeHandle());
    }
}