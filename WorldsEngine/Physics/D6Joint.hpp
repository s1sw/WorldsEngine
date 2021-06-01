#pragma once
#include <entt/entity/fwd.hpp>

namespace physx {
    class PxD6Joint;
    class PxRigidActor;
}

namespace worlds {
    enum class D6Motion {
        Locked,
        Limited,
        Free
    };

    enum class D6Axis {
    };

    struct D6Joint {
        D6Joint();
        D6Joint(const D6Joint&) = delete;
        D6Joint(D6Joint&& other) noexcept;
        void operator=(D6Joint&& other);
        physx::PxD6Joint* pxJoint;
        bool reverseJoint = false;

        void setTarget(entt::entity newTargetEnt, entt::registry& reg);
        void setAllLinearMotion(D6Motion motion);
        void setAllAngularMotion(D6Motion motion);
        entt::entity getTarget();

        ~D6Joint();
    private:
        friend void setupD6Joint(entt::registry&, entt::entity);
        physx::PxRigidActor* thisActor;
        entt::entity targetEntity;
    };
}
