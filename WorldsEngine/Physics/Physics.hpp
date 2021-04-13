#pragma once
#include <glm/gtx/quaternion.hpp>
#include <physx/PxPhysics.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxFoundation.h>
#include <physx/extensions/PxD6Joint.h>
#include "../Core/Transform.hpp"
#include "PhysicsActor.hpp"

namespace worlds {
    extern physx::PxMaterial* defaultMaterial;
    extern physx::PxScene* g_scene;
    extern physx::PxPhysics* g_physics;

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

            shape->setContactOffset(0.01f);
            shape->setRestOffset(0.005f);
            shape->setLocalPose(physx::PxTransform{ glm2px(ps.pos * scale), glm2px(ps.rot) });

            pa.actor->attachShape(*shape);
            shape->release();
        }
    }

    struct RaycastHitInfo {
        entt::entity entity;
        glm::vec3 normal;
        glm::vec3 worldPos;
    };

    bool raycast(physx::PxVec3 position, physx::PxVec3 direction, float maxDist = FLT_MAX, RaycastHitInfo* hitInfo = nullptr);
    bool raycast(glm::vec3 position, glm::vec3 direction, float maxDist = FLT_MAX, RaycastHitInfo* hitInfo = nullptr);
    void initPhysx(entt::registry& reg);
    void stepSimulation(float deltaTime);
    void shutdownPhysx();
}
