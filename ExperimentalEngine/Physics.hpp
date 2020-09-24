#pragma once
#include <glm/gtx/quaternion.hpp>
#include <physx/PxPhysics.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxFoundation.h>
#include <physx/extensions/PxDefaultErrorCallback.h>
#include <physx/extensions/PxDefaultAllocator.h>
#include <physx/pvd/PxPvdTransport.h>
#include <physx/pvd/PxPvd.h>
#include "Transform.hpp"
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

    inline physx::PxTransform glm2px(Transform& t) {
        return physx::PxTransform(glm2px(t.position), glm2px(t.rotation));
    }

    inline void updateMass(DynamicPhysicsActor& pa) {
        physx::PxRigidBodyExt::setMassAndUpdateInertia(*(physx::PxRigidBody*)pa.actor, pa.mass);
    }

    template <typename T>
    void updatePhysicsShapes(T& pa) {
        uint32_t nShapes = pa.actor->getNbShapes();
        physx::PxShape** buf = (physx::PxShape**)std::malloc(nShapes * sizeof(physx::PxShape*));
        pa.actor->getShapes(buf, nShapes);

        for (int i = 0; i < nShapes; i++) {
            pa.actor->detachShape(*buf[i]);
            buf[i]->release();
        }

        std::free(buf);

        for (PhysicsShape& ps : pa.physicsShapes) {
            physx::PxShape* shape;

            switch (ps.type) {
            case PhysicsShapeType::Box:
                shape = g_physics->createShape(physx::PxBoxGeometry(glm2px(ps.box.halfExtents)), *(ps.material == nullptr ? defaultMaterial : ps.material));
                break;
            default:
                ps.sphere.radius = 0.5f;
            case PhysicsShapeType::Sphere:
                shape = g_physics->createShape(physx::PxSphereGeometry(ps.sphere.radius), *(ps.material == nullptr ? defaultMaterial : ps.material));
                break;
            case PhysicsShapeType::Capsule:
                shape = g_physics->createShape(physx::PxCapsuleGeometry(ps.capsule.radius, ps.capsule.height * 0.5f), *(ps.material == nullptr ? defaultMaterial : ps.material));
                break;
            }

            shape->setContactOffset(0.01f);
            shape->setRestOffset(0.005f);

            pa.actor->attachShape(*shape);
        }
    }

    void initPhysx(entt::registry& reg);
    void stepSimulation(float deltaTime);
    void shutdownPhysx();
}