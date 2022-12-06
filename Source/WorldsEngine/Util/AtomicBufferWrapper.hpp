#pragma once
#include <stdint.h>
#include <atomic>

namespace worlds
{
    template <typename T>
    struct AtomicBufferWrapper
    {
        std::atomic<uint32_t> CurrentLoc = 0;
        T* Buffer;

        AtomicBufferWrapper(T* Buffer)
            : Buffer(Buffer)
        {
        }

        void Clear()
        {
            CurrentLoc = 0;
        }

        uint32_t Append(T val)
        {
            uint32_t location = CurrentLoc.fetch_add(1);
            Buffer[location] = val;
            return location;
        }
    };
}