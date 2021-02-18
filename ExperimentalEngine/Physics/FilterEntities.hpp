#pragma once
#include "PxRigidActor.h"
#include "PxQueryFiltering.h"
#include <entt/entity/entity.hpp>

namespace worlds {
	struct FilterEntities : public physx::PxQueryFilterCallback {
        uint32_t ents[8] = { 0 };
        uint32_t numFilterEnts = 0;

        physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&, const physx::PxShape*, const physx::PxRigidActor*, physx::PxHitFlags&) override {
            return physx::PxQueryHitType::eBLOCK;
        }


        physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit& hit) override {
            for (uint32_t i = 0; i < numFilterEnts; i++) {
                if ((uint32_t)(uintptr_t)hit.actor->userData == ents[i])
                    return physx::PxQueryHitType::eNONE;
            }
            return physx::PxQueryHitType::eBLOCK;
        }
	};
}
