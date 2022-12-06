#pragma once
#include <entt/entity/lw_fwd.hpp>
#include <glm/mat4x4.hpp>

namespace R2::VK
{
    class CommandBuffer;
    class Texture;
}

namespace worlds
{
    class VKRTTPass;

    class IRenderPipeline
    {
      public:
        virtual void setup(VKRTTPass* rttPass) = 0;
        virtual void onResize(int width, int height) = 0;
        virtual void draw(entt::registry& reg, R2::VK::CommandBuffer& cb) = 0;
        virtual void setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix) {}
        virtual R2::VK::Texture* getHDRTexture() { return nullptr; }
        virtual ~IRenderPipeline() = default;
    };
}