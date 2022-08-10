#include <R2/VKCommandBuffer.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/VKTexture.hpp>
#include <R2/VKPipeline.hpp>
#include <volk.h>

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

    void CommandBuffer::BindGraphicsDescriptorSet(PipelineLayout* pipelineLayout, VkDescriptorSet set, uint32_t setNumber)
    {
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout->GetNativeHandle(),
            setNumber, 1, &set, 0, nullptr);
    }

    void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        vkCmdDrawIndexed(cb, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        vkCmdDraw(cb, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void CommandBuffer::BindComputePipeline(Pipeline* p)
    {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, p->GetNativeHandle());
    }

    void CommandBuffer::BindComputeDescriptorSet(PipelineLayout* pipelineLayout, VkDescriptorSet set, uint32_t setNumber)
    {
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->GetNativeHandle(),
            setNumber, 1, &set, 0, nullptr);
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

    VkCommandBuffer CommandBuffer::GetNativeHandle()
    {
        return cb;
    }
}
