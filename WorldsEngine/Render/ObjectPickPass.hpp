#pragma once
#include <entt/entity/lw_fwd.hpp>
#include <Render/Camera.hpp>
#include <Render/PickParams.hpp>
#include <Util/UniquePtr.hpp>

namespace R2::VK
{
    class Core;
    class CommandBuffer;
    class Pipeline;
    class PipelineLayout;
    class DescriptorSet;
    class DescriptorSetLayout;
    class Buffer;
    class Texture;
    class Event;
}

namespace worlds
{
    class VKRenderer;

    class ObjectPickPass
    {
    public:
        explicit ObjectPickPass(VKRenderer* renderer);
        void requestPick(PickParams params);
        void execute(R2::VK::CommandBuffer& cb, entt::registry& reg);
        bool getResult(uint32_t& entityID);
    private:
        VKRenderer* renderer;
        UniquePtr<R2::VK::Event> pickEvent;
        UniquePtr<R2::VK::Pipeline> pickPipeline;
        UniquePtr<R2::VK::PipelineLayout> pickPipelineLayout;
        UniquePtr<R2::VK::DescriptorSetLayout> dsl;
        UniquePtr<R2::VK::DescriptorSet> ds;
        UniquePtr<R2::VK::Buffer> pickBuffer;
        UniquePtr<R2::VK::Texture> depthTex;

        PickParams params;
        bool doPick = false;
    };
}
