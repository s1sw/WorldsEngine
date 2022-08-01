#pragma once
#include <stdint.h>

namespace R2::VK
{
    class Buffer;
    class Texture;
    class Core;
    class CommandBuffer;
}

namespace worlds
{
    typedef uint32_t AssetID;

    class SimpleCompute
    {
    public:
        SimpleCompute(R2::VK::Core* core, AssetID shaderId);
        ~SimpleCompute();
        SimpleCompute& BindBuffer(const char* bindPoint, R2::VK::Buffer* buffer);
        SimpleCompute& BindSampledTexture(const char* bindPoint, R2::VK::Texture* texture);
        SimpleCompute& BindStorageTexture(const char* bindPoint, R2::VK::Texture* texture);
        void UpdateDescriptorSet();
        void Dispatch(R2::VK::CommandBuffer& cb, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);
    };
}