#pragma once
#include "PxQueryFiltering.h"
#include "PxRigidActor.h"
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

namespace worlds
{
    struct FilterEntities : public physx::PxQueryFilterCallback
    {
        uint32_t ents[8] = {0};
        uint32_t numFilterEnts = 0;

        physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&, const physx::PxShape*,
                                              const physx::PxRigidActor*, physx::PxHitFlags&) override
        {
            return physx::PxQueryHitType::eBLOCK;
        }

        physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit& hit) override
        {
            for (uint32_t i = 0; i < numFilterEnts; i++)
            {
                if ((uint32_t)(uintptr_t)hit.actor->userData == ents[i])
                    return physx::PxQueryHitType::eNONE;
            }
            return physx::PxQueryHitType::eBLOCK;
        }
    };

    template <typename T> class FilterComponent : public physx::PxQueryFilterCallback
    {
        entt::registry& registry;

      public:
        FilterComponent(entt::registry& registry) : registry(registry)
        {
        }

        physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&, const physx::PxShape*,
                                              const physx::PxRigidActor*, physx::PxHitFlags&) override
        {
            return physx::PxQueryHitType::eBLOCK;
        }

        physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit& hit) override
        {
            entt::entity ent = (entt::entity)(uint32_t)(uintptr_t)hit.actor->userData;
            if (!registry.has<T>(ent))
                return physx::PxQueryHitType::eNONE;
            return physx::PxQueryHitType::eBLOCK;
        }
    };
}
