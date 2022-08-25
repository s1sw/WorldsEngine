#pragma once

namespace R2::VK
{
    class Core;
    class Buffer;
    struct BufferCreateInfo;

    class FrameSeparatedBuffer
    {
        Buffer* buffers[2];
        Core* core;
    public:
        FrameSeparatedBuffer(Core* core, const BufferCreateInfo& bci);
        ~FrameSeparatedBuffer();
        Buffer* GetBuffer(int index);
        Buffer* GetCurrentBuffer();
    };
}