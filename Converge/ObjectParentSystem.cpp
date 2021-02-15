#include "ObjectParentSystem.hpp"
#include <Core/Transform.hpp>

namespace converge {
    void ObjectParentSystem::update(entt::registry& registry, float, float) {
        registry.view<ParentComponent, Transform>().each([&](auto, auto& pc, auto& tf) {
            if (!registry.valid(pc.parent))
                return;

            auto& parentTf = registry.get<Transform>(pc.parent);
            tf.position = parentTf.position + pc.posOffset;
            tf.rotation = parentTf.rotation * pc.rotOffset;
        });

    }
}
