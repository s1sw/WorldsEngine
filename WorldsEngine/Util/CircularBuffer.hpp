#pragma once

namespace worlds
{
    template <typename T, size_t sz> struct CircularBuffer
    {
        T values[sz];
        size_t idx;

        void add(T value)
        {
            values[idx] = value;
            idx++;
            if (idx >= sz)
                idx = 0;
        }

        size_t size() const
        {
            return sz;
        }
    };
}