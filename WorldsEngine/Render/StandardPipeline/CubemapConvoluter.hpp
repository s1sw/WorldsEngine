#pragma once
#include <Util/UniquePtr.hpp>

namespace R2::VK
{
    class Core;
    class Pipeline;
    class PipelineLayout;
    class Texture;
    class CommandBuffer;
    class Sampler;
}

namespace worlds
{
    class SimpleCompute;

    class CubemapConvoluter
    {
        R2::VK::Core* core;
        UniquePtr<R2::VK::Sampler> sampler;
    public:
        CubemapConvoluter(R2::VK::Core* core);
        void Convolute(R2::VK::CommandBuffer cb, R2::VK::Texture* tex);
    };
}