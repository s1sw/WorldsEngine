#pragma once
#include <entt/entity/lw_fwd.hpp>

namespace R2::VK {
    class CommandBuffer;
}

namespace worlds {
    class VKRTTPass;

    class IRenderPipeline {
    public:
        virtual void setup(VKRTTPass* rttPass) = 0;
        virtual void onResize(int width, int height) = 0;
        virtual void draw(entt::registry& reg, R2::VK::CommandBuffer& cb) = 0;
        virtual ~IRenderPipeline() = default;
    };
}