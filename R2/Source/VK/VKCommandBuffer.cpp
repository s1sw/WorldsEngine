#include <volk.h>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/VKSyncPrims.hpp>
#include <R2/VKTexture.hpp>
#include <R2/VKPipeline.hpp>

namespace R2::VK
{
    CommandBuffer::CommandBuffer(VkCommandBuffer cb)
        : cb(cb)
    {

    }

    void CommandBuffer::SetViewport(Viewport vp)
    {
        VkViewport vkv{};
        vkv.x = vp.X;
        vkv.y = vp.Y;
        vkv.width = vp.Width;
        vkv.height = vp.Height;
        vkv.minDepth = vp.MinDepth;
        vkv.maxDepth = vp.MaxDepth;
        vkCmdSetViewport(cb, 0, 1, &vkv);
    }

    void CommandBuffer::SetScissor(ScissorRect rect)
    {
        VkRect2D vks{};
        vks.offset.x = rect.X;
        vks.offset.y = rect.Y;
        vks.extent.width = rect.Width;
        vks.extent.height = rect.Height;
        vkCmdSetScissor(cb, 0, 1, &vks);
    }

    void CommandBuffer::ClearScissor()
    {
        vkCmdSetScissor(cb, 0, 0, nullptr);
    }

    void CommandBuffer::BindVertexBuffer(uint32_t location, Buffer* buffer, uint64_t offset)
    {
        VkBuffer b = buffer->GetNativeHandle();
        vkCmdBindVertexBuffers(cb, location, 1, &b, &offset);
    }

    void CommandBuffer::BindIndexBuffer(Buffer* buffer, uint64_t offset, IndexType indexType)
    {
        vkCmdBindIndexBuffer(cb, buffer->GetNativeHandle(), offset, static_cast<VkIndexType>(indexType));
    }

    void CommandBuffer::BindPipeline(Pipeline* p)
    {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, p->GetNativeHandle());
    }

    void CommandBuffer::BindGraphicsDescriptorSet(PipelineLayout* pipelineLayout, DescriptorSet* set, uint32_t setNumber)
    {
        VkDescriptorSet vkSet = set->GetNativeHandle();
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout->GetNativeHandle(),
            setNumber, 1, &vkSet, 0, nullptr);
    }

    void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        vkCmdDrawIndexed(cb, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void CommandBuffer::DrawIndexedIndirect(Buffer* buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
    {
        vkCmdDrawIndexedIndirect(cb, buffer->GetNativeHandle(), offset, drawCount, stride);
    }

    void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        vkCmdDraw(cb, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void CommandBuffer::BindComputePipeline(Pipeline* p)
    {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, p->GetNativeHandle());
    }

    void CommandBuffer::BindComputeDescriptorSet(PipelineLayout* pipelineLayout, DescriptorSet* set, uint32_t setNumber)
    {
        VkDescriptorSet vkSet = set->GetNativeHandle();
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->GetNativeHandle(),
            setNumber, 1, &vkSet, 0, nullptr);
    }

    void CommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        vkCmdDispatch(cb, groupCountX, groupCountY, groupCountZ);
    }

    void CommandBuffer::PushConstants(const void* data, size_t dataSize, ShaderStage stages, PipelineLayout* pipelineLayout)
    {
        vkCmdPushConstants(cb, pipelineLayout->GetNativeHandle(),
            static_cast<VkShaderStageFlagBits>(stages), 0, (uint32_t)dataSize, data);
    }

    void CommandBuffer::BeginDebugLabel(const char* label, float r, float g, float b)
    {
        // If a layer that uses debug labels isn't present,
        // both the debug label begin and end functions will be null.
        if (vkCmdBeginDebugUtilsLabelEXT)
        {
            VkDebugUtilsLabelEXT labelObj{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
            labelObj.pLabelName = label;
            labelObj.color[0] = r;
            labelObj.color[1] = g;
            labelObj.color[2] = b;
            labelObj.color[3] = 1.0f;

            vkCmdBeginDebugUtilsLabelEXT(cb, &labelObj);
        }
    }

    void CommandBuffer::EndDebugLabel()
    {
        if (vkCmdEndDebugUtilsLabelEXT)
        {
            vkCmdEndDebugUtilsLabelEXT(cb);
        }
    }

    void CommandBuffer::TextureBarrier(Texture* tex, PipelineStageFlags srcStage, PipelineStageFlags dstStage, AccessFlags srcAccess, AccessFlags dstAccess)
    {
        VkImageMemoryBarrier2 imageBarrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        imageBarrier.image = tex->GetNativeHandle();
        imageBarrier.oldLayout = imageBarrier.newLayout = (VkImageLayout)tex->lastLayout;
        tex->lastAccess = dstAccess;
        imageBarrier.srcStageMask = (VkPipelineStageFlags2)srcStage;
        imageBarrier.dstStageMask = (VkPipelineStageFlags2)dstStage;
        imageBarrier.srcAccessMask = (VkAccessFlags2)srcAccess;
        imageBarrier.dstAccessMask = (VkAccessFlags2)dstAccess;
        imageBarrier.subresourceRange = VkImageSubresourceRange { tex->getAspectFlags(), 0, (uint32_t)tex->GetNumMips(), 0, (uint32_t)tex->GetLayers() };

        VkDependencyInfo di { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        di.imageMemoryBarrierCount = 1;
        di.pImageMemoryBarriers = &imageBarrier;
        di.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        vkCmdPipelineBarrier2(cb, &di);
    }

    VkOffset3D convertOffset(BlitOffset offset)
    {
        return VkOffset3D { offset.X, offset.Y, offset.Z };
    }

    VkExtent3D convertExtent(BlitExtent extent)
    {
        return VkExtent3D { extent.X, extent.Y, extent.Z };
    }

    void CommandBuffer::TextureBlit(Texture* source, Texture* destination, R2::VK::TextureBlit blitInfo)
    {
        source->Acquire(*this, ImageLayout::TransferSrcOptimal, AccessFlags::TransferRead, PipelineStageFlags::Transfer);
        destination->Acquire(*this, ImageLayout::TransferDstOptimal, AccessFlags::TransferWrite, PipelineStageFlags::Transfer);

        VkImageBlit imageBlit{};
        imageBlit.srcSubresource.aspectMask = source->getAspectFlags();
        imageBlit.srcSubresource.baseArrayLayer = blitInfo.Source.LayerStart;
        imageBlit.srcSubresource.layerCount = blitInfo.Source.LayerCount;
        imageBlit.srcSubresource.mipLevel = blitInfo.Source.MipLevel;

        imageBlit.dstSubresource.aspectMask = destination->getAspectFlags();
        imageBlit.dstSubresource.baseArrayLayer = blitInfo.Destination.LayerStart;
        imageBlit.dstSubresource.layerCount = blitInfo.Destination.LayerCount;
        imageBlit.dstSubresource.mipLevel = blitInfo.Destination.MipLevel;

        imageBlit.srcOffsets[0] = convertOffset(blitInfo.SourceOffsets[0]);
        imageBlit.srcOffsets[1] = convertOffset(blitInfo.SourceOffsets[1]);

        imageBlit.dstOffsets[0] = convertOffset(blitInfo.DestinationOffsets[0]);
        imageBlit.dstOffsets[1] = convertOffset(blitInfo.DestinationOffsets[1]);

        vkCmdBlitImage(cb,
            source->GetNativeHandle(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            destination->GetNativeHandle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &imageBlit,
            VK_FILTER_LINEAR
        );
    }

    void CommandBuffer::TextureCopy(Texture* source, Texture* destination, R2::VK::TextureCopy copyInfo)
    {
        source->Acquire(*this, ImageLayout::TransferSrcOptimal, AccessFlags::TransferRead, PipelineStageFlags::Transfer);
        destination->Acquire(*this, ImageLayout::TransferDstOptimal, AccessFlags::TransferWrite, PipelineStageFlags::Transfer);

        VkImageCopy imageCopy{};
        imageCopy.srcSubresource.aspectMask = source->getAspectFlags();
        imageCopy.srcSubresource.baseArrayLayer = copyInfo.Source.LayerStart;
        imageCopy.srcSubresource.layerCount = copyInfo.Source.LayerCount;
        imageCopy.srcSubresource.mipLevel = copyInfo.Source.MipLevel;

        imageCopy.dstSubresource.aspectMask = destination->getAspectFlags();
        imageCopy.dstSubresource.baseArrayLayer = copyInfo.Destination.LayerStart;
        imageCopy.dstSubresource.layerCount = copyInfo.Destination.LayerCount;
        imageCopy.dstSubresource.mipLevel = copyInfo.Destination.MipLevel;

        imageCopy.srcOffset = convertOffset(copyInfo.SourceOffset);
        imageCopy.dstOffset = convertOffset(copyInfo.DestinationOffset);

        imageCopy.extent = convertExtent(copyInfo.Extent);

        vkCmdCopyImage(cb,
            source->GetNativeHandle(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            destination->GetNativeHandle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &imageCopy
        );
    }

    void CommandBuffer::TextureCopyToBuffer(Texture* source, Buffer* destination)
    {
        source->Acquire(cb, ImageLayout::TransferSrcOptimal, AccessFlags::TransferRead, PipelineStageFlags::Transfer);
        VkBufferImageCopy bic{};
        bic.imageSubresource.layerCount = 1;
        bic.imageSubresource.aspectMask = source->getAspectFlags();
        bic.imageExtent = VkExtent3D { (uint32_t)source->GetWidth(), (uint32_t)source->GetHeight(), 1 };

        vkCmdCopyImageToBuffer(
            cb,
            source->GetNativeHandle(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            destination->GetNativeHandle(),
            1, &bic
        );
    }

    VkCommandBuffer CommandBuffer::GetNativeHandle()
    {
        return cb;
    }

    void CommandBuffer::UpdateBuffer(Buffer *buffer, uint64_t offset, uint64_t size, void *data)
    {
        vkCmdUpdateBuffer(cb, buffer->GetNativeHandle(), offset, size, data);
    }

    void CommandBuffer::FillBuffer(Buffer *buffer, uint64_t offset, uint64_t size, uint32_t data)
    {
        vkCmdFillBuffer(cb, buffer->GetNativeHandle(), offset, size, data);
    }

    void CommandBuffer::SetEvent(Event *evt)
    {
        vkCmdSetEvent(cb, evt->GetNativeHandle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }

    void CommandBuffer::ResetEvent(R2::VK::Event *evt)
    {
        vkCmdResetEvent(cb, evt->GetNativeHandle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }
}
