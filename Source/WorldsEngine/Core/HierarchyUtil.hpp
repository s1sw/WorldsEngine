#pragma once
#include <entt/entity/lw_fwd.hpp>

namespace worlds
{
    class HierarchyUtil
    {
      public:
        static bool isEntityChildOf(entt::registry& registry, entt::entity object, entt::entity parent);
        static void setEntityParent(entt::registry& registry, entt::entity object, entt::entity parent);
        static void removeEntityParent(entt::registry& registry, entt::entity object, bool removeChildComponent = true);
    };
}