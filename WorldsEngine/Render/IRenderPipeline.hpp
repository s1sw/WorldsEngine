#pragma once

namespace R2::VK {
    class CommandBuffer;
}

namespace worlds {
    class IRenderPipeline {
    public:
        void setup(RTTPass* rttPass);
        void onResize(int width, int height);
        void draw(entt::registry& reg, R2::VK::CommandBuffer& cb);
    };
}