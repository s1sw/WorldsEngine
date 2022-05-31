#pragma once
#include <glm/gtx/quaternion.hpp>
#include <physx/PxPhysics.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxFoundation.h>
#include <physx/extensions/PxD6Joint.h>
#include "../Core/Transform.hpp"
#include "Core/IGameEventHandler.hpp"
#include "Core/Log.hpp"
#include "PhysicsActor.hpp"
#include <entt/entity/fwd.hpp>
#include <functional>
#include <Core/MeshManager.hpp>

namespace worlds {
    const uint32_t DEFAULT_PHYSICS_LAYER = 1;
    const uint32_t PLAYER_PHYSICS_LAYER = 2;
    const uint32_t NOCOLLISION_PHYSICS_LAYER = 4;

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

    inline void updateMass(RigidBody& pa) {
        physx::PxRigidBodyExt::setMassAndUpdateInertia(*(physx::PxRigidBody*)pa.actor, pa.mass);
        pa.actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !pa.enableGravity);
        pa.actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, pa.enableCCD);
    }


    struct RaycastHitInfo {
        entt::entity entity;
        glm::vec3 normal;
        glm::vec3 worldPos;
        float distance;
    };

    typedef void (*ContactModCallback)(void* ctx, physx::PxContactModifyPair* pairs, uint32_t count);

    class PhysicsSystem {
    public:
        PhysicsSystem(const EngineInterfaces& interfaces, entt::registry& reg);
        void stepSimulation(float deltaTime);
        bool raycast(glm::vec3 position, glm::vec3 direction, float maxDist = FLT_MAX, RaycastHitInfo* hitInfo = nullptr, uint32_t excludedLayers = 0u);
        uint32_t overlapSphereMultiple(glm::vec3 origin, float radius, uint32_t maxTouchCount, entt::entity* hitEntityBuffer, uint32_t excludedLayers = 0u);
        bool sweepSphere(glm::vec3 origin, float radius, glm::vec3 direction, float distance, RaycastHitInfo* hitInfo = nullptr, uint32_t excludedLayers = 0u);
        void setContactModCallback(void* ctx, ContactModCallback callback);

        template <typename T>
        void updatePhysicsShapes(T& pa, glm::vec3 scale = glm::vec3{ 1.0f });

        void resetMeshCache();

        physx::PxScene* scene() { return _scene; }
        physx::PxPhysics* physics() { return _physics; }
        ~PhysicsSystem();
    private:
        void setupD6Joint(entt::registry& reg, entt::entity ent);
        void destroyD6Joint(entt::registry& reg, entt::entity ent);
        void setupFixedJoint(entt::registry& reg, entt::entity ent);
        void destroyFixedJoint(entt::registry& reg, entt::entity ent);
        entt::registry& reg;
        physx::PxMaterial* _defaultMaterial;
        physx::PxScene* _scene;
        physx::PxPhysics* _physics;
        physx::PxCooking* _cooking;
        physx::PxFoundation* foundation;
        physx::PxDefaultAllocator allocator;
        physx::PxErrorCallback* errorCallback;
        physx::PxRigidBody* dummyBody;
    };

    struct PhysicsContactInfo {
        float relativeSpeed;
        entt::entity otherEntity;
        glm::vec3 averageContactPoint;
        glm::vec3 normal;
    };

    struct PhysicsEvents {
        using ContactFunc = std::function<void(entt::entity, const PhysicsContactInfo&)>;
        static const uint32_t MAX_CONTACT_EVENTS = 4;

        PhysicsEvents() : onContact { } {}

        uint32_t addContactCallback(ContactFunc func) {
            for (uint32_t i = 0; i < MAX_CONTACT_EVENTS; i++) {
                if (onContact[i] == nullptr) {
                    onContact[i] = func;
                    return i;
                }
            }

            logErr("Exhausted contact callbacks");
            return ~0u;
        }

        void removeContactCallback(uint32_t index) {
            for (uint32_t i = 0; i < MAX_CONTACT_EVENTS; i++) {
                if (i == index) {
                    onContact[i] = nullptr;
                    break;
                }
            }
        }

        ContactFunc onContact[MAX_CONTACT_EVENTS] = { nullptr, nullptr, nullptr, nullptr };
    };
}
