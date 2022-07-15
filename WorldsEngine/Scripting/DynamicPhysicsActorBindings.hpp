#include "Export.hpp"
#include "Physics/Physics.hpp"
#include <entt/entity/registry.hpp>

using namespace worlds;

extern "C"
{
    EXPORT int dynamicpa_getShapeCount(entt::registry* reg, entt::entity entity)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        return (int)dpa.physicsShapes.size();
    }

    EXPORT void dynamicpa_getShape(entt::registry* reg, entt::entity entity, int shapeIndex, PhysicsShape* shape)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);
        *shape = dpa.physicsShapes[shapeIndex];
    }

    EXPORT void dynamicpa_setShapeCount(entt::registry* reg, entt::entity entity, int shapeCount)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        dpa.physicsShapes.resize(shapeCount);
    }

    EXPORT void dynamicpa_setShape(entt::registry* reg, entt::entity entity, int shapeIndex, PhysicsShape* shape)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        dpa.physicsShapes[shapeIndex] = *shape;
    }

    EXPORT void dynamicpa_updateShapes(entt::registry* reg, entt::entity entity)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);
        Transform& t = reg->get<Transform>(entity);

        csharpInterfaces->physics->updatePhysicsShapes(dpa, t.scale);
        updateMass(dpa);
    }

    EXPORT void dynamicpa_addForce(entt::registry* reg, entt::entity entity, glm::vec3 force, ForceMode forceMode)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        dpa.addForce(force, forceMode);
    }

    EXPORT void dynamicpa_addTorque(entt::registry* reg, entt::entity entity, glm::vec3 torque, ForceMode forceMode)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        dpa.addTorque(torque, forceMode);
    }

    EXPORT void dynamicpa_addForceAtPosition(entt::registry* reg, entt::entity entity, glm::vec3 force, glm::vec3 pos,
                                             ForceMode forceMode)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        dpa.addForceAtPosition(force, pos, forceMode);
    }

    EXPORT void dynamicpa_getPose(entt::registry* reg, entt::entity entity, Transform* pose)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        *pose = dpa.pose();
    }

    EXPORT void dynamicpa_setPose(entt::registry* reg, entt::entity entity, Transform* pose)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        dpa.setPose(*pose);
    }

    EXPORT void dynamicpa_getLinearVelocity(entt::registry* reg, entt::entity entity, glm::vec3* vel)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        *vel = dpa.linearVelocity();
    }

    EXPORT void dynamicpa_setLinearVelocity(entt::registry* reg, entt::entity entity, glm::vec3 vel)
    {
        reg->get<RigidBody>(entity).setLinearVelocity(vel);
    }

    EXPORT void dynamicpa_getAngularVelocity(entt::registry* reg, entt::entity entity, glm::vec3* vel)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        *vel = dpa.angularVelocity();
    }

    EXPORT void dynamicpa_setAngularVelocity(entt::registry* reg, entt::entity entity, glm::vec3 vel)
    {
        reg->get<RigidBody>(entity).setAngularVelocity(vel);
    }

    EXPORT float dynamicpa_getMass(entt::registry* reg, entt::entity entity)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        return dpa.mass;
    }

    EXPORT void dynamicpa_setMass(entt::registry* reg, entt::entity entity, float mass)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        dpa.mass = mass;
    }

    EXPORT void dynamicpa_getCenterOfMassLocalPose(entt::registry* reg, entt::entity entity, Transform* pose)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        *pose = px2glm(dpa.actor->getCMassLocalPose());
    }

    EXPORT void dynamicpa_getMassSpaceInertiaTensor(entt::registry* reg, entt::entity entity, glm::vec3* tensor)
    {
        RigidBody& dpa = reg->get<RigidBody>(entity);

        *tensor = px2glm(dpa.actor->getMassSpaceInertiaTensor());
    }

    EXPORT void dynamicpa_setMaxAngularVelocity(entt::registry* reg, entt::entity entity, float vel)
    {
        reg->get<RigidBody>(entity).setMaxAngularVelocity(vel);
    }

    EXPORT float dynamicpa_getMaxAngularVelocity(entt::registry* reg, entt::entity entity)
    {
        return reg->get<RigidBody>(entity).maxAngularVelocity();
    }

    EXPORT void dynamicpa_setMaxLinearVelocity(entt::registry* reg, entt::entity entity, float vel)
    {
        reg->get<RigidBody>(entity).setMaxLinearVelocity(vel);
    }

    EXPORT float dynamicpa_getMaxLinearVelocity(entt::registry* reg, entt::entity entity)
    {
        return reg->get<RigidBody>(entity).maxLinearVelocity();
    }

    EXPORT bool dynamicpa_getKinematic(entt::registry* reg, entt::entity entity)
    {
        return (reg->get<RigidBody>(entity).actor->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC) ==
               physx::PxRigidBodyFlag::eKINEMATIC;
    }

    EXPORT void dynamicpa_setKinematic(entt::registry* reg, entt::entity entity, bool kinematic)
    {
        reg->get<RigidBody>(entity).actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, kinematic);
    }

    EXPORT bool dynamicpa_getUseContactMod(entt::registry* reg, entt::entity entity)
    {
        return reg->get<RigidBody>(entity).useContactMod;
    }

    EXPORT void dynamicpa_setUseContactMod(entt::registry* reg, entt::entity entity, bool useContactMod)
    {
        reg->get<RigidBody>(entity).useContactMod = useContactMod;
    }

    EXPORT float dynamicpa_getContactOffset(entt::registry* reg, entt::entity entity)
    {
        return reg->get<RigidBody>(entity).contactOffset;
    }

    EXPORT void dynamicpa_setContactOffset(entt::registry* reg, entt::entity entity, float value)
    {
        reg->get<RigidBody>(entity).contactOffset = value;
    }

    EXPORT float dynamicpa_getSleepThreshold(entt::registry* reg, entt::entity entity)
    {
        return reg->get<RigidBody>(entity).actor->getSleepThreshold();
    }

    EXPORT void dynamicpa_setSleepThreshold(entt::registry* reg, entt::entity entity, float value)
    {
        reg->get<RigidBody>(entity).actor->setSleepThreshold(value);
    }

    EXPORT bool dynamicpa_getEnabled(entt::registry* reg, entt::entity entity)
    {
        return reg->get<RigidBody>(entity).enabled();
    }

    EXPORT void dynamicpa_setEnabled(entt::registry* reg, entt::entity entity, bool value)
    {
        reg->get<RigidBody>(entity).setEnabled(value);
    }
}
