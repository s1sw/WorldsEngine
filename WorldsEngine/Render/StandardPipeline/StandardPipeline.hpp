#pragma once
#include <Render/IRenderPipeline.hpp>

namespace R2::VK {
    class DescriptorSetLayout;
    class DescriptorSet;
    class Pipeline;
    class PipelineLayout;
    class Buffer;
    class Texture;
    class Core;
}

namespace worlds {
    class VKRenderer;

    class StandardPipeline : public IRenderPipeline {
    };
}