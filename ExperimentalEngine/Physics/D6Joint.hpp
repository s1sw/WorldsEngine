#pragma once
#include <entt/entity/fwd.hpp>

namespace physx {
    class PxD6Joint;
    class PxRigidActor;
}

namespace worlds {
    struct D6Joint {
        D6Joint();
        D6Joint(const D6Joint&) = delete;
        D6Joint(D6Joint&& other) noexcept;
        void operator=(D6Joint&& other);
        physx::PxD6Joint* pxJoint;

        void setTarget(entt::entity newTargetEnt, entt::registry& reg);
        entt::entity getTarget();

        ~D6Joint();
    private:
        friend void setupD6Joint(entt::registry&, entt::entity);
        physx::PxRigidActor* thisActor;
        entt::entity targetEntity;
    };
}