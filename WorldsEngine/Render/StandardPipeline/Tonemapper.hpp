#pragma once

namespace R2::VK
{
    class Texture;
    class Pipeline;
    class PipelineLayout;
}

namespace worlds
{
    class Tonemapper
    {
        R2::VK::Texture* colorBuffer;
        R2::VK::Texture* target;
        R2::VK::PipelineLayout* pipelineLayout;
        R2::VK::Pipeline* pipeline;
    public:
        Tonemapper(R2::VK::Texture* colorBuffer, R2::VK::Texture* target);
    };
}