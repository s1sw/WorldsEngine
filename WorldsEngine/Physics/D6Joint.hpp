#pragma once
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace physx
{
    class PxD6Joint;
    class PxRigidActor;
}

namespace worlds
{
    enum class D6Motion
    {
        Locked,
        Limited,
        Free
    };

    enum class D6Axis
    {
    };

    struct D6Joint
    {
        D6Joint();
        D6Joint(const D6Joint &) = delete;
        D6Joint(D6Joint &&other) noexcept;
        void operator=(D6Joint &&other);
        physx::PxD6Joint *pxJoint = nullptr;
        bool reverseJoint = false;

        void setTarget(entt::entity newTargetEnt, entt::registry &reg);
        void setAllLinearMotion(D6Motion motion);
        void setAllAngularMotion(D6Motion motion);
        entt::entity getTarget();

        entt::entity getAttached();
        void setAttached(entt::entity entity, entt::registry &reg);

        ~D6Joint();

      private:
        void updateJointActors();
        friend class PhysicsSystem;
        friend class D6JointEditor;
        physx::PxRigidActor *thisActor = nullptr;
        physx::PxRigidActor *originalThisActor = nullptr;
        physx::PxRigidActor *targetActor = nullptr;
        entt::entity targetEntity;
        entt::entity replaceThis;
    };
}
