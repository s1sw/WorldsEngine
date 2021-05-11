#pragma once
#include <glm/gtx/quaternion.hpp>
#include <physx/PxPhysics.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxFoundation.h>
#include <physx/extensions/PxD6Joint.h>
#include "../Core/Transform.hpp"
#include "PhysicsActor.hpp"
#include <entt/entity/fwd.hpp>
#include <functional>

namespace worlds {
    extern physx::PxMaterial* defaultMaterial;
    extern physx::PxScene* g_scene;
    extern physx::PxPhysics* g_physics;

    const uint32_t DEFAULT_PHYSICS_LAYER = 0;
    const uint32_t PLAYER_PHYSICS_LAYER = 1;
    const uint32_t NOCOLLISION_PHYSICS_LAYER = 2;

    inline physx::PxVec3 glm2px(glm::vec3 vec) {
        return physx::PxVec3(vec.x, vec.y, vec.z);
    }

    inline physx::PxQuat glm2px(glm::quat quat) {
        return physx::PxQuat{ quat.x, quat.y, quat.z, quat.w };
    }

    inline glm::vec3 px2glm(physx::PxVec3 vec) {
        return glm::vec3(vec.x, vec.y, vec.z);
    }

    inline glm::quat px2glm(physx::PxQuat quat) {
        return glm::quat{ quat.w, quat.x, quat.y, quat.z };
    }

    inline Transform px2glm(const physx::PxTransform& t) {
        return Transform{px2glm(t.p), px2glm(t.q)};
    }

    inline physx::PxTransform glm2px(const Transform& t) {
        return physx::PxTransform(glm2px(t.position), glm2px(t.rotation));
    }

    inline void updateMass(DynamicPhysicsActor& pa) {
        physx::PxRigidBodyExt::setMassAndUpdateInertia(*(physx::PxRigidBody*)pa.actor, pa.mass);
    }

    template <typename T>
    void updatePhysicsShapes(T& pa, glm::vec3 scale = glm::vec3{1.0f}) {
        uint32_t nShapes = pa.actor->getNbShapes();
        physx::PxShape** buf = (physx::PxShape**)std::malloc(nShapes * sizeof(physx::PxShape*));
        pa.actor->getShapes(buf, nShapes);

        for (uint32_t i = 0; i < nShapes; i++) {
            pa.actor->detachShape(*buf[i]);
        }

        std::free(buf);

        for (PhysicsShape& ps : pa.physicsShapes) {
            physx::PxShape* shape;
            physx::PxMaterial* mat = ps.material ? ps.material : defaultMaterial;

            switch (ps.type) {
            case PhysicsShapeType::Box:
                shape = g_physics->createShape(
                    physx::PxBoxGeometry(glm2px(ps.box.halfExtents * scale)),
                    *mat
                );
                break;
            default:
                ps.sphere.radius = 0.5f;
            case PhysicsShapeType::Sphere:
                shape = g_physics->createShape(
                    physx::PxSphereGeometry(ps.sphere.radius * glm::compAdd(scale) / 3.0f),
                    *mat
                );
                break;
            case PhysicsShapeType::Capsule:
                shape = g_physics->createShape(
                    physx::PxCapsuleGeometry(ps.capsule.radius, ps.capsule.height * 0.5f),
                    *mat
                );
                break;
            }

            shape->setLocalPose(physx::PxTransform{ glm2px(ps.pos * scale), glm2px(ps.rot) });
            physx::PxFilterData data;
            data.word0 = pa.layer;
            shape->setSimulationFilterData(data);
            shape->setQueryFilterData(data);

            pa.actor->attachShape(*shape);
            shape->release();
        }
    }

    struct RaycastHitInfo {
        entt::entity entity;
        glm::vec3 normal;
        glm::vec3 worldPos;
        float distance;
    };

    bool raycast(physx::PxVec3 position, physx::PxVec3 direction, float maxDist = FLT_MAX, RaycastHitInfo* hitInfo = nullptr, uint32_t excludeLayer = ~0u);
    bool raycast(glm::vec3 position, glm::vec3 direction, float maxDist = FLT_MAX, RaycastHitInfo* hitInfo = nullptr, uint32_t excludeLayer = ~0u);
    void initPhysx(entt::registry& reg);
    void stepSimulation(float deltaTime);
    void shutdownPhysx();

    struct PhysicsContactInfo {
        float relativeSpeed;
        entt::entity otherEntity;
        glm::vec3 averageContactPoint;
    };

    struct PhysicsEvents {
        using ContactFunc = std::function<void(entt::entity, const PhysicsContactInfo&)>;
        static const uint32_t MAX_CONTACT_EVENTS = 4;

        PhysicsEvents() : onContact { } {}

        void addContactCallback(ContactFunc func) {
            for (int i = 0; i < 4; i++) {
                if (onContact[i] == nullptr) {
                    onContact[i] = func;
                    break;
                }
            }
        }

        ContactFunc onContact[MAX_CONTACT_EVENTS] = { nullptr, nullptr, nullptr, nullptr };
    };
}
