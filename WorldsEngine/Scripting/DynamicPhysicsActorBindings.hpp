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

    EXPORT void dynamicpa_addForce(entt::registry* reg, entt::entity entity, glm::vec3 force, ForceMode forceMode) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        dpa.addForce(force, forceMode);
    }

    EXPORT void dynamicpa_addTorque(entt::registry* reg, entt::entity entity, glm::vec3 torque, ForceMode forceMode) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        dpa.addTorque(torque, forceMode);
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

    EXPORT float dynamicpa_getMass(entt::registry* reg, entt::entity entity) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        return dpa.mass;
    }

    EXPORT void dynamicpa_setMass(entt::registry* reg, entt::entity entity, float mass) {
        DynamicPhysicsActor& dpa = reg->get<DynamicPhysicsActor>(entity);

        dpa.mass = mass;
    }
}
