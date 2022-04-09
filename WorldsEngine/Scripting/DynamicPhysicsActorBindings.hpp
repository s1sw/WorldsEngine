#include <entt/entity/registry.hpp>
#include "Physics/Physics.hpp"
#include "Export.hpp"

using namespace worlds;

extern "C" {
    EXPORT int dynamicpa_getShapeCount(entt::registry* reg, entt::entity entity) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        return (int)dpa.physicsShapes.size();
    }

    EXPORT void dynamicpa_getShape(entt::registry* reg, entt::entity entity, int shapeIndex, PhysicsShape* shape) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);
        *shape = dpa.physicsShapes[shapeIndex];
    }

    EXPORT void dynamicpa_setShapeCount(entt::registry* reg, entt::entity entity, int shapeCount) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        dpa.physicsShapes.resize(shapeCount);
    }

    EXPORT void dynamicpa_setShape(entt::registry* reg, entt::entity entity, int shapeIndex, PhysicsShape* shape) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        dpa.physicsShapes[shapeIndex] = *shape;
    }

    EXPORT void dynamicpa_updateShapes(entt::registry* reg, entt::entity entity) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);
        Transform& t = reg->get<Transform>(entity);

        csharpInterfaces->physics->updatePhysicsShapes(dpa, t.scale);
        updateMass(dpa);
    }

    EXPORT void dynamicpa_addForce(entt::registry* reg, entt::entity entity, glm::vec3 force, ForceMode forceMode) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        dpa.addForce(force, forceMode);
    }

    EXPORT void dynamicpa_addTorque(entt::registry* reg, entt::entity entity, glm::vec3 torque, ForceMode forceMode) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        dpa.addTorque(torque, forceMode);
    }

    EXPORT void dynamicpa_addForceAtPosition(entt::registry* reg, entt::entity entity, glm::vec3 force, glm::vec3 pos, ForceMode forceMode) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        dpa.addForceAtPosition(force, pos, forceMode);
    }

    EXPORT void dynamicpa_getPose(entt::registry* reg, entt::entity entity, Transform* pose) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        *pose = dpa.pose();
    }

    EXPORT void dynamicpa_setPose(entt::registry* reg, entt::entity entity, Transform* pose) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        dpa.setPose(*pose);
    }

    EXPORT void dynamicpa_getLinearVelocity(entt::registry* reg, entt::entity entity, glm::vec3* vel) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        *vel = dpa.linearVelocity();
    }

    EXPORT void dynamicpa_setLinearVelocity(entt::registry* reg, entt::entity entity, glm::vec3 vel) {
        reg->get<DynamicPhysicsActor>(entity).setLinearVelocity(vel);
    }

    EXPORT void dynamicpa_getAngularVelocity(entt::registry* reg, entt::entity entity, glm::vec3* vel) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        *vel = dpa.angularVelocity();
    }

    EXPORT void dynamicpa_setAngularVelocity(entt::registry* reg, entt::entity entity, glm::vec3 vel) {
        reg->get<DynamicPhysicsActor>(entity).setAngularVelocity(vel);
    }

    EXPORT float dynamicpa_getMass(entt::registry* reg, entt::entity entity) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        return dpa.mass;
    }

    EXPORT void dynamicpa_setMass(entt::registry* reg, entt::entity entity, float mass) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        dpa.mass = mass;
    }

    EXPORT void dynamicpa_getCenterOfMassLocalPose(entt::registry* reg, entt::entity entity, Transform* pose) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        *pose = px2glm(dpa.actor->getCMassLocalPose());
    }

    EXPORT void dynamicpa_getMassSpaceInertiaTensor(entt::registry* reg, entt::entity entity, glm::vec3* tensor) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        *tensor = px2glm(dpa.actor->getMassSpaceInertiaTensor());
    }

    EXPORT void dynamicpa_setMaxAngularVelocity(entt::registry* reg, entt::entity entity, float vel) {
        reg->get<DynamicPhysicsActor>(entity).setMaxAngularVelocity(vel);
    }

    EXPORT float dynamicpa_getMaxAngularVelocity(entt::registry* reg, entt::entity entity) {
        return reg->get<DynamicPhysicsActor>(entity).maxAngularVelocity();
    }

    EXPORT void dynamicpa_setMaxLinearVelocity(entt::registry* reg, entt::entity entity, float vel) {
        reg->get<DynamicPhysicsActor>(entity).setMaxLinearVelocity(vel);
    }

    EXPORT float dynamicpa_getMaxLinearVelocity(entt::registry* reg, entt::entity entity) {
        return reg->get<DynamicPhysicsActor>(entity).maxLinearVelocity();
    }

    EXPORT bool dynamicpa_getKinematic(entt::registry* reg, entt::entity entity) {
        return (reg->get<DynamicPhysicsActor>(entity).actor->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC) == physx::PxRigidBodyFlag::eKINEMATIC;
    }

    EXPORT void dynamicpa_setKinematic(entt::registry* reg, entt::entity entity, bool kinematic) {
        reg->get<DynamicPhysicsActor>(entity).actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, kinematic);
    }

    EXPORT bool dynamicpa_getUseContactMod(entt::registry* reg, entt::entity entity) {
        return reg->get<DynamicPhysicsActor>(entity).useContactMod;
    }

    EXPORT void dynamicpa_setUseContactMod(entt::registry* reg, entt::entity entity, bool useContactMod) {
        reg->get<DynamicPhysicsActor>(entity).useContactMod = useContactMod;
    }

    EXPORT float dynamicpa_getContactOffset(entt::registry* reg, entt::entity entity) {
        return reg->get<DynamicPhysicsActor>(entity).contactOffset;
    }

    EXPORT void dynamicpa_setContactOffset(entt::registry* reg, entt::entity entity, float value) {
        reg->get<DynamicPhysicsActor>(entity).contactOffset = value;
    }
}
