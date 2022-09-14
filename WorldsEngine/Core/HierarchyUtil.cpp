#include "HierarchyUtil.hpp"
#include <Core/Log.hpp>
#include <Core/WorldComponents.hpp>
#include <assert.h>
#include <entt/entity/registry.hpp>

namespace worlds
{
    bool HierarchyUtil::isEntityChildOf(entt::registry& reg, entt::entity object, entt::entity parent)
    {
        ParentComponent* pc = reg.try_get<ParentComponent>(parent);

        if (pc == nullptr)
            return false;

        entt::entity currentChild = pc->firstChild;
        while (currentChild != entt::null)
        {
            if (currentChild == object)
                return true;
            ChildComponent& cc = reg.get<ChildComponent>(currentChild);

            if (isEntityChildOf(reg, object, currentChild))
                return true;

            currentChild = cc.nextChild;
        }

        return false;
    }

    void HierarchyUtil::setEntityParent(entt::registry& reg, entt::entity object, entt::entity parent)
    {
        if (reg.has<ChildComponent>(object))
        {
            removeEntityParent(reg, object);
        }

        if (!reg.valid(parent))
        {
            if (parent != entt::null)
                logWarn("setEntityParent called with invalid parent");
            return;
        }

        Transform parentTransform = reg.get<Transform>(parent);

        auto& childComponent = reg.emplace<ChildComponent>(object);
        childComponent.offset = reg.get<Transform>(object).transformByInverse(parentTransform);
        childComponent.offset.scale = reg.get<Transform>(object).scale;
        childComponent.parent = parent;

        if (reg.has<ParentComponent>(parent))
        {
            auto& parentComponent = reg.get<ParentComponent>(parent);
            childComponent.nextChild = parentComponent.firstChild;

            auto& oldChildComponent = reg.get<ChildComponent>(parentComponent.firstChild);
            oldChildComponent.prevChild = object;

            parentComponent.firstChild = object;
        }
        else
        {
            reg.emplace<ParentComponent>(parent, object);
        }
    }

    void HierarchyUtil::removeEntityParent(entt::registry& reg, entt::entity object, bool removeChildComponent)
    {
        assert(reg.has<ChildComponent>(object));

        auto& childComponent = reg.get<ChildComponent>(object);

        entt::entity parent = childComponent.parent;
        childComponent.parent = entt::null;

        if (!reg.has<ParentComponent>(parent))
            return;

        auto& parentComponent = reg.get<ParentComponent>(parent);

        if (childComponent.nextChild == entt::null && childComponent.prevChild == entt::null)
        {
            // the parent has no other children
            // it is no longer a parent.
            if (reg.has<ParentComponent>(parent))
                reg.remove<ParentComponent>(parent);

            if (removeChildComponent)
                reg.remove<ChildComponent>(object);

            return;
        }

        if (parentComponent.firstChild == object)
        {
            parentComponent.firstChild = childComponent.nextChild;
        }

        if (childComponent.nextChild != entt::null)
        {
            auto& nextChildComponent = reg.get<ChildComponent>(childComponent.nextChild);
            nextChildComponent.prevChild = childComponent.prevChild;
        }

        if (childComponent.prevChild != entt::null)
        {
            auto& prevChildComponent = reg.get<ChildComponent>(childComponent.prevChild);
            prevChildComponent.nextChild = childComponent.nextChild;
        }

        if (removeChildComponent)
            reg.remove<ChildComponent>(object);
    }
}