#include "Export.hpp"
#include <entt/entity/registry.hpp>
#include "Physics/Physics.hpp"
#include <Physics/D6Joint.hpp>

using namespace worlds;

extern "C" {
    EXPORT uint32_t physics_raycast(glm::vec3 origin, glm::vec3 direction, float maxDist, uint32_t excludeLayerMask, RaycastHitInfo* hitInfo) {
        return (uint32_t)raycast(origin, direction, maxDist, hitInfo, excludeLayerMask);
    }

    EXPORT uint32_t physics_overlapSphere(glm::vec3 origin, float radius, uint32_t* entPtr) {
        // search for nearby grabbable objects
        physx::PxSphereGeometry sphereGeo{ radius };
        physx::PxOverlapBuffer hit;
        physx::PxQueryFilterData filterData;
        filterData.flags = physx::PxQueryFlag::eDYNAMIC
            | physx::PxQueryFlag::eSTATIC
            | physx::PxQueryFlag::eANY_HIT;

        physx::PxTransform t{ physx::PxIdentity };
        t.p = glm2px(origin);

        bool overlapped = g_scene->overlap(sphereGeo, t, hit, filterData);

        if (overlapped) {
            const auto& touch = hit.getAnyHit(0);
            *entPtr = (uint32_t)(uintptr_t)touch.actor->userData;
        }

        return (uint32_t)overlapped;
    }

    EXPORT uint32_t physics_overlapSphereMultiple(glm::vec3 origin, float radius, uint32_t maxTouchCount, uint32_t* hitEntityBuffer, uint32_t excludeLayerMask) {
        return overlapSphereMultiple(origin, radius, maxTouchCount, hitEntityBuffer, excludeLayerMask);
    }

    EXPORT physx::PxMaterial* physicsmaterial_new(float staticFriction, float dynamicFriction, float restitution) {
        return g_physics->createMaterial(staticFriction, dynamicFriction, restitution);
    }

    EXPORT void physicsmaterial_acquireReference(physx::PxMaterial* material) {
        material->acquireReference();
    }

    EXPORT void physicsmaterial_release(physx::PxMaterial* material) {
        material->release();
    }

    EXPORT float physicsmaterial_getStaticFriction(physx::PxMaterial* material) {
        return material->getStaticFriction();
    }

    EXPORT float physicsmaterial_getDynamicFriction(physx::PxMaterial* material) {
        return material->getDynamicFriction();
    }

    EXPORT float physicsmaterial_getRestitution(physx::PxMaterial* material) {
        return material->getRestitution();
    }

    EXPORT void physicsmaterial_setStaticFriction(physx::PxMaterial* material, float val) {
        material->setStaticFriction(val);
    }

    EXPORT void physicsmaterial_setDynamicFriction(physx::PxMaterial* material, float val) {
        material->setDynamicFriction(val);
    }

    EXPORT void physicsmaterial_setRestitution(physx::PxMaterial* material, float val) {
        material->setRestitution(val);
    }

    EXPORT void physicsmaterial_setFrictionCombineMode(physx::PxMaterial* material, physx::PxCombineMode::Enum mode) {
        material->setFrictionCombineMode(mode);
    }

    EXPORT physx::PxCombineMode::Enum physicsmaterial_getFrictionCombineMode(physx::PxMaterial* material) {
        return material->getFrictionCombineMode();
    }

    EXPORT void d6joint_setTarget(entt::registry* reg, entt::entity d6Entity, entt::entity target) {
        reg->get<D6Joint>(d6Entity).setTarget(target, *reg);
    }

    EXPORT uint32_t d6joint_getTarget(entt::registry* reg, entt::entity entity) {
        return (uint32_t)reg->get<D6Joint>(entity).getTarget();
    }

    EXPORT void d6joint_setAxisMotion(entt::registry* reg, entt::entity entity, physx::PxD6Axis::Enum axis, physx::PxD6Motion::Enum motion) {
        reg->get<D6Joint>(entity).pxJoint->setMotion(axis, motion);
    }

    EXPORT void d6joint_getLocalPose(entt::registry* reg, entt::entity entity, physx::PxJointActorIndex::Enum actorIndex, Transform* t) {
        *t = px2glm(reg->get<D6Joint>(entity).pxJoint->getLocalPose(actorIndex));
    }

    EXPORT void d6joint_setLocalPose(entt::registry* reg, entt::entity entity, physx::PxJointActorIndex::Enum actorIndex, Transform* t) {
        reg->get<D6Joint>(entity).pxJoint->setLocalPose(actorIndex, glm2px(*t));
    }

    EXPORT void d6joint_setLinearLimit(entt::registry* reg, entt::entity entity, physx::PxD6Axis::Enum axis, physx::PxJointLinearLimitPair* limit) {
        reg->get<D6Joint>(entity).pxJoint->setLinearLimit(axis, *limit);
    }
    
    EXPORT void d6joint_setDrive(entt::registry* reg, entt::entity entity, physx::PxD6Drive::Enum axis, physx::PxD6JointDrive* drive) {
        reg->get<D6Joint>(entity).pxJoint->setDrive(axis, *drive);
    }
}
