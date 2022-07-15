#pragma once
#include <entt/entity/fwd.hpp>

namespace physx
{
    class PxFixedJoint;
    class PxRigidActor;
}

namespace worlds
{
    struct FixedJoint
    {
        FixedJoint();
        FixedJoint(const FixedJoint &) = delete;
        FixedJoint(FixedJoint &&other) noexcept;
        void operator=(FixedJoint &&other);
        physx::PxFixedJoint *pxJoint;

        void setTarget(entt::entity newTargetEnt, entt::registry &reg);
        entt::entity getTarget();

        ~FixedJoint();

      private:
        friend class PhysicsSystem;
        physx::PxRigidActor *thisActor;
        entt::entity targetEntity;
    };
}
