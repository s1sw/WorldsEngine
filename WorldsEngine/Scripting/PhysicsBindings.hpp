#include "Export.hpp"
#include "Physics/Physics.hpp"

using namespace worlds;

extern "C" {
    EXPORT uint32_t physics_raycast(glm::vec3 origin, glm::vec3 direction, float maxDist, RaycastHitInfo* hitInfo) {
        return (uint32_t)raycast(origin, direction, maxDist, hitInfo);
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
}
