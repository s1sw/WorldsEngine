#pragma once

namespace R2::VK
{
    class Core;
    class Pipeline;
    class PipelineLayout;
}

namespace worlds
{
    class CubemapConvoluter
    {
        R2::VK::Core* core;
    public:
        CubemapConvoluter(R2::VK::Core* core);
    };
}