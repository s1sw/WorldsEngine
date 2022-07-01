#include <R2/VKCommandBuffer.hpp>
#include <R2/VKBuffer.hpp>
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

    void CommandBuffer::BindGraphicsDescriptorSet(VkPipelineLayout pipelineLayout, VkDescriptorSet set, uint32_t setNumber)
    {
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, setNumber, 1, &set, 0, nullptr);
    }

    void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        vkCmdDrawIndexed(cb, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void CommandBuffer::PushConstants(const void* data, size_t dataSize, ShaderStage stages, VkPipelineLayout pipelineLayout)
    {
        vkCmdPushConstants(cb, pipelineLayout, static_cast<VkShaderStageFlagBits>(stages), 0, (uint32_t)dataSize, data);
    }

    VkCommandBuffer CommandBuffer::GetNativeHandle()
    {
        return cb;
    }

    void CommandBuffer::BeginDebugLabel(const char* label, float r, float g, float b)
    {
        VkDebugUtilsLabelEXT labelObj{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
        labelObj.pLabelName = label;
        labelObj.color[0] = r;
        labelObj.color[1] = g;
        labelObj.color[2] = b;
        labelObj.color[3] = 1.0f;

        vkCmdBeginDebugUtilsLabelEXT(cb, &labelObj);
    }

    void CommandBuffer::EndDebugLabel()
    {
        vkCmdEndDebugUtilsLabelEXT(cb);
    }
}
