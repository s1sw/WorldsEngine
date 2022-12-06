#include "Export.hpp"
#include "Physics/Physics.hpp"
#include <Physics/D6Joint.hpp>
#include <Util/MathsUtil.hpp>
#include <entt/entity/registry.hpp>

using namespace worlds;
extern EngineInterfaces const* csharpInterfaces;
extern "C"
{
    EXPORT uint32_t physics_raycast(glm::vec3 origin, glm::vec3 direction, float maxDist, uint32_t excludeLayerMask,
                                    RaycastHitInfo* hitInfo)
    {
        return (uint32_t)csharpInterfaces->physics->raycast(origin, direction, maxDist, hitInfo, excludeLayerMask);
    }

    EXPORT uint32_t physics_overlapSphere(glm::vec3 origin, float radius, uint32_t* entPtr)
    {
        // search for nearby grabbable objects
        physx::PxSphereGeometry sphereGeo{radius};
        physx::PxOverlapBuffer hit;
        physx::PxQueryFilterData filterData;
        filterData.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eANY_HIT;

        physx::PxTransform t{physx::PxIdentity};
        t.p = glm2px(origin);

        bool overlapped = csharpInterfaces->physics->scene()->overlap(sphereGeo, t, hit, filterData);

        if (overlapped)
        {
            const auto& touch = hit.getAnyHit(0);
            *entPtr = (uint32_t)(uintptr_t)touch.actor->userData;
        }

        return (uint32_t)overlapped;
    }

    EXPORT uint32_t physics_overlapSphereMultiple(glm::vec3 origin, float radius, uint32_t maxTouchCount,
                                                  uint32_t* hitEntityBuffer, uint32_t excludeLayerMask)
    {
        return csharpInterfaces->physics->overlapSphereMultiple(
            origin, radius, maxTouchCount, reinterpret_cast<entt::entity*>(hitEntityBuffer), excludeLayerMask);
    }

    EXPORT bool physics_sweepSphere(glm::vec3 origin, float radius, glm::vec3 direction, float distance,
                                    RaycastHitInfo* hitInfo, uint32_t excludeLayerMask)
    {
        return csharpInterfaces->physics->sweepSphere(origin, radius, direction, distance, hitInfo, excludeLayerMask);
    }

    EXPORT void physics_setContactModCallback(void* ctx, ContactModCallback callback)
    {
        csharpInterfaces->physics->setContactModCallback(ctx, callback);
    }

    EXPORT physx::PxMaterial* physicsmaterial_new(float staticFriction, float dynamicFriction, float restitution)
    {
        return csharpInterfaces->physics->physics()->createMaterial(staticFriction, dynamicFriction, restitution);
    }

    EXPORT void physicsmaterial_acquireReference(physx::PxMaterial* material)
    {
        material->acquireReference();
    }

    EXPORT void physicsmaterial_release(physx::PxMaterial* material)
    {
        material->release();
    }

    EXPORT float physicsmaterial_getStaticFriction(physx::PxMaterial* material)
    {
        return material->getStaticFriction();
    }

    EXPORT float physicsmaterial_getDynamicFriction(physx::PxMaterial* material)
    {
        return material->getDynamicFriction();
    }

    EXPORT float physicsmaterial_getRestitution(physx::PxMaterial* material)
    {
        return material->getRestitution();
    }

    EXPORT void physicsmaterial_setStaticFriction(physx::PxMaterial* material, float val)
    {
        material->setStaticFriction(val);
    }

    EXPORT void physicsmaterial_setDynamicFriction(physx::PxMaterial* material, float val)
    {
        material->setDynamicFriction(val);
    }

    EXPORT void physicsmaterial_setRestitution(physx::PxMaterial* material, float val)
    {
        material->setRestitution(val);
    }

    EXPORT void physicsmaterial_setFrictionCombineMode(physx::PxMaterial* material, physx::PxCombineMode::Enum mode)
    {
        material->setFrictionCombineMode(mode);
    }

    EXPORT physx::PxCombineMode::Enum physicsmaterial_getFrictionCombineMode(physx::PxMaterial* material)
    {
        return material->getFrictionCombineMode();
    }

    EXPORT void d6joint_setTarget(entt::registry* reg, entt::entity d6Entity, entt::entity target)
    {
        reg->get<D6Joint>(d6Entity).setTarget(target, *reg);
    }

    EXPORT uint32_t d6joint_getTarget(entt::registry* reg, entt::entity entity)
    {
        return (uint32_t)reg->get<D6Joint>(entity).getTarget();
    }

    EXPORT void d6joint_setAxisMotion(entt::registry* reg, entt::entity entity, physx::PxD6Axis::Enum axis,
                                      physx::PxD6Motion::Enum motion)
    {
        reg->get<D6Joint>(entity).pxJoint->setMotion(axis, motion);
    }

    EXPORT void d6joint_getLocalPose(entt::registry* reg, entt::entity entity,
                                     physx::PxJointActorIndex::Enum actorIndex, Transform* t)
    {
        *t = px2glm(reg->get<D6Joint>(entity).pxJoint->getLocalPose(actorIndex));
    }

    EXPORT void d6joint_setLocalPose(entt::registry* reg, entt::entity entity,
                                     physx::PxJointActorIndex::Enum actorIndex, Transform* t)
    {
        reg->get<D6Joint>(entity).pxJoint->setLocalPose(actorIndex, glm2px(*t));
    }

    EXPORT void d6joint_setLinearLimit(entt::registry* reg, entt::entity entity, physx::PxD6Axis::Enum axis,
                                       physx::PxJointLinearLimitPair* limit)
    {
        reg->get<D6Joint>(entity).pxJoint->setLinearLimit(axis, *limit);
    }

    EXPORT void d6joint_setTwistLimit(entt::registry* reg, entt::entity entity, physx::PxJointAngularLimitPair& limit)
    {
        reg->get<D6Joint>(entity).pxJoint->setTwistLimit(limit);
    }

    EXPORT void d6joint_setPyramidSwingLimit(entt::registry* reg, entt::entity entity, physx::PxJointLimitPyramid& limit)
    {
        reg->get<D6Joint>(entity).pxJoint->setPyramidSwingLimit(limit);
    }

    EXPORT void d6joint_setSwingLimit(entt::registry* reg, entt::entity entity, physx::PxJointLimitCone& limit)
    {
        reg->get<D6Joint>(entity).pxJoint->setSwingLimit(limit);
    }

    EXPORT void d6joint_setDrive(entt::registry* reg, entt::entity entity, physx::PxD6Drive::Enum axis,
                                 physx::PxD6JointDrive* drive)
    {
        reg->get<D6Joint>(entity).pxJoint->setDrive(axis, *drive);
    }

    EXPORT void d6joint_setBreakForce(entt::registry* reg, entt::entity entity, float breakForce)
    {
        D6Joint& j = reg->get<D6Joint>(entity);
        float force, torque;
        j.pxJoint->getBreakForce(force, torque);
        j.pxJoint->setBreakForce(breakForce, torque);
    }

    EXPORT float d6joint_getBreakForce(entt::registry* reg, entt::entity entity)
    {
        D6Joint& j = reg->get<D6Joint>(entity);
        float force, torque;
        j.pxJoint->getBreakForce(force, torque);
        return force;
    }

    EXPORT bool d6joint_isBroken(entt::registry* reg, entt::entity entity)
    {
        D6Joint& j = reg->get<D6Joint>(entity);
        return (j.pxJoint->getConstraintFlags() & physx::PxConstraintFlag::eBROKEN);
    }

    EXPORT entt::entity d6joint_getAttached(entt::registry* reg, entt::entity entity)
    {
        D6Joint& j = reg->get<D6Joint>(entity);
        return j.getAttached();
    }

    EXPORT void d6joint_setAttached(entt::registry* reg, entt::entity entity, entt::entity attached)
    {
        D6Joint& j = reg->get<D6Joint>(entity);
        j.setAttached(attached, *reg);
    }

    EXPORT entt::entity ContactModifyPair_getEntity(physx::PxContactModifyPair* pair, int idx)
    {
        return (entt::entity)(uint32_t)(uintptr_t)(pair->actor[idx]->userData);
    }

    EXPORT void ContactModifyPair_getTransform(physx::PxContactModifyPair* pair, int idx, Transform* t)
    {
        *t = px2glm(pair->transform[idx]);
    }

    EXPORT physx::PxContactSet* ContactModifyPair_getContactSetPointer(physx::PxContactModifyPair* pair)
    {
        return &pair->contacts;
    }

    EXPORT void ContactSet_getTargetVelocity(physx::PxContactSet* contactSet, int idx, glm::vec3* value)
    {
        *value = px2glm(contactSet->getTargetVelocity(idx));
    }

    EXPORT void ContactSet_setTargetVelocity(physx::PxContactSet* contactSet, int idx, glm::vec3 value)
    {
        contactSet->setTargetVelocity(idx, glm2px(value));
    }

    EXPORT void ContactSet_getNormal(physx::PxContactSet* contactSet, int idx, glm::vec3* value)
    {
        *value = px2glm(contactSet->getNormal(idx));
    }

    EXPORT void ContactSet_setNormal(physx::PxContactSet* contactSet, int idx, glm::vec3 value)
    {
        contactSet->setNormal(idx, glm2px(value));
    }

    EXPORT float ContactSet_getMaxImpulse(physx::PxContactSet* contactSet, int idx)
    {
        return contactSet->getMaxImpulse(idx);
    }

    EXPORT void ContactSet_setMaxImpulse(physx::PxContactSet* contactSet, int idx, float val)
    {
        contactSet->setMaxImpulse(idx, val);
    }

    EXPORT float ContactSet_getDynamicFriction(physx::PxContactSet* contactSet, int idx)
    {
        return contactSet->getDynamicFriction(idx);
    }

    EXPORT void ContactSet_setDynamicFriction(physx::PxContactSet* contactSet, int idx, float val)
    {
        contactSet->setDynamicFriction(idx, val);
    }

    EXPORT float ContactSet_getStaticFriction(physx::PxContactSet* contactSet, int idx)
    {
        return contactSet->getStaticFriction(idx);
    }

    EXPORT void ContactSet_setStaticFriction(physx::PxContactSet* contactSet, int idx, float val)
    {
        contactSet->setStaticFriction(idx, val);
    }

    EXPORT float ContactSet_getRestitution(physx::PxContactSet* contactSet, int idx)
    {
        return contactSet->getRestitution(idx);
    }

    EXPORT void ContactSet_setRestitution(physx::PxContactSet* contactSet, int idx, float val)
    {
        contactSet->setRestitution(idx, val);
    }

    EXPORT float ContactSet_getSeparation(physx::PxContactSet* contactSet, int idx)
    {
        return contactSet->getSeparation(idx);
    }

    EXPORT void ContactSet_setSeparation(physx::PxContactSet* contactSet, int idx, float val)
    {
        contactSet->setSeparation(idx, val);
    }

    EXPORT void ContactSet_getPoint(physx::PxContactSet* contactSet, int idx, glm::vec3* val)
    {
        *val = px2glm(contactSet->getPoint(idx));
    }

    EXPORT uint32_t ContactSet_getCount(physx::PxContactSet* contactSet)
    {
        return contactSet->size();
    }

    EXPORT int ContactModifyPair_getSize()
    {
        return sizeof(physx::PxContactModifyPair);
    }
}
