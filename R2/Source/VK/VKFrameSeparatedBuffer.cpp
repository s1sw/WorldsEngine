#include <R2/VKFrameSeparatedBuffer.hpp>
#include <R2/VK.hpp>

namespace R2::VK
{
    FrameSeparatedBuffer::FrameSeparatedBuffer(Core* core, const BufferCreateInfo& bci)
        : core(core)
    {
        for (int i = 0; i < 2; i++)
        {
            buffers[i] = core->CreateBuffer(bci);
        }
    }

    FrameSeparatedBuffer::~FrameSeparatedBuffer()
    {
        for (int i = 0; i < 2; i++)
        {
            core->DestroyBuffer(buffers[i]);
        }
    }

    Buffer* FrameSeparatedBuffer::GetBuffer(int index)
    {
        return buffers[index];
    }

    Buffer* FrameSeparatedBuffer::GetCurrentBuffer()
    {
        return buffers[core->GetFrameIndex()];
    }

    void* FrameSeparatedBuffer::MapCurrent()
    {
        return GetCurrentBuffer()->Map();
    }

    void FrameSeparatedBuffer::UnmapCurrent()
    {
        GetCurrentBuffer()->Unmap();
    }
}