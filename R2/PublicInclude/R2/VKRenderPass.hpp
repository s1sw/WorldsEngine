#pragma once
#include <stdint.h>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VkCommandBuffer)
#undef VK_DEFINE_HANDLE

namespace R2::VK
{
    class Texture;

    enum class LoadOp
    {
        Load,
        Clear,
        DontCare
    };

    enum class StoreOp
    {
        Store,
        DontCare
    };

    union ClearColorValue
    {
        float       Float32[4];
        int32_t     Int32[4];
        uint32_t    Uint32[4];
    };

    struct ClearDepthStencilValue
    {
        float       Depth;
        uint32_t    stencil;
    };

    union ClearValue
    {
        ClearColorValue           Color;
        ClearDepthStencilValue    DepthStencil;

        static ClearValue FloatColorClear(float r, float g, float b, float a)
        {
            ClearValue cv;
            cv.Color.Float32[0] = r;
            cv.Color.Float32[1] = g;
            cv.Color.Float32[2] = b;
            cv.Color.Float32[3] = a;

            return cv;
        }

        static ClearValue DepthClear(float depth)
        {
            ClearValue cv;
            cv.DepthStencil.Depth = depth;
            return cv;
        }
    };

    class CommandBuffer;

    class RenderPass
    {
    public:
        RenderPass();
        RenderPass& RenderArea(uint32_t width, uint32_t height);

        RenderPass& ColorAttachment(Texture* tex, LoadOp loadOp, StoreOp storeOp);
        RenderPass& ColorAttachmentClearValue(ClearValue val);
        RenderPass& DepthAttachment(Texture* tex, LoadOp loadOp, StoreOp storeOp);
        RenderPass& DepthAttachmentClearValue(ClearValue val);

        RenderPass& ViewMask(uint32_t viewMask);

        void Begin(CommandBuffer cb);
        void End(CommandBuffer cb);
    private:
        struct AttachmentInfo
        {
            Texture* Texture;
            LoadOp LoadOp;
            StoreOp StoreOp;
            ClearValue ClearValue;
        };

        AttachmentInfo colorAttachments[4];
        uint32_t numColorAttachments;
        AttachmentInfo depthAttachment;
        uint32_t width;
        uint32_t height;
        uint32_t viewMask;
    };
}