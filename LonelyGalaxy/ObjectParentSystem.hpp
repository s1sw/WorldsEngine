#pragma once
#include <Core/ISystem.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lg {
    struct ParentComponent {
        ParentComponent() 
            : parent {entt::null}
            , posOffset {0.0f}
            , rotOffset {} {}
        ParentComponent(entt::entity p, 
                        glm::vec3 pOffset = { 0.0f, 0.0f, 0.0f }, 
                        glm::quat rOffset = {})
            : parent(p)
            , posOffset(pOffset)
            , rotOffset(rOffset) {
        }
        entt::entity parent;
        glm::vec3 posOffset;
        glm::quat rotOffset;
    };

    class ObjectParentSystem : public worlds::ISystem {
    public:
        void update(entt::registry& registry, float deltaTime, float interpAlpha) override;
    };
}

