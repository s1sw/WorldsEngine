#pragma once
#include "PxRigidActor.h"
#include "PxQueryFiltering.h"
#include <entt/entity/entity.hpp>

namespace worlds {
	struct FilterEntities : public physx::PxQueryFilterCallback {
        entt::entity ents[8] = { entt::null, entt::null, entt::null, entt::null, entt::null, entt::null, entt::null, entt::null };

        physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&, const physx::PxShape*, const physx::PxRigidActor* actor, physx::PxHitFlags&) override {
            for (int i = 0; i < 8; i++) {
                if ((uint32_t)(uintptr_t)actor->userData == (uint32_t)ents[i])
                    return physx::PxQueryHitType::eNONE;
            }
            return physx::PxQueryHitType::eBLOCK;
        }


        physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit& hit) override {
            for (int i = 0; i < 8; i++) {
                if ((uint32_t)(uintptr_t)hit.actor->userData == (uint32_t)ents[i])
                    return physx::PxQueryHitType::eNONE;
            }
            return physx::PxQueryHitType::eBLOCK;
        }
	};
}
