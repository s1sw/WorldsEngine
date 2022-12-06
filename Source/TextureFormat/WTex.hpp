#pragma once
#include <stddef.h>
#include <stdint.h>

namespace wtex
{
    typedef uint64_t OffsetType;

    enum class ContainedFormat
    {
        RGBA,
        Crunch
    };

#pragma pack(push, 1)
    struct Header
    {
        char magic[4] = {'W', 'T', 'E', 'X'};
        int32_t version = 1;
        ContainedFormat containedFormat;
        int32_t width;
        int32_t height;
        int32_t numMipLevels;
        bool isSrgb;
        uint64_t dataSize;
        OffsetType dataOffset;

        bool verifyMagic()
        {
            return magic[0] == 'W' && magic[1] == 'T' && magic[2] == 'E' && magic[3] == 'X';
        }

        void *getRelPtr(OffsetType offset)
        {
            return ((char *)this) + offset;
        }

        void *getData()
        {
            return getRelPtr(dataOffset);
        }
    };
#pragma pack(pop)
}
