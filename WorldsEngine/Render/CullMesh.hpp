#pragma once
#include <Render/Frustum.hpp>
#include <Render/RenderInternal.hpp>

namespace worlds
{
    inline bool cullMesh(const RenderMeshInfo& rmi, const Transform& t, Frustum* frustums, int numViews)
    {
        float maxScale = glm::max(t.scale.x, glm::max(t.scale.y, t.scale.z));
        bool reject = true;
        for (int i = 0; i < numViews; i++)
        {
            if (frustums[i].containsSphere(t.position, maxScale * rmi.boundingSphereRadius))
                reject = false;
        }

        if (reject)
            return false;

        reject = false;
        AABB aabb = AABB{rmi.aabbMin, rmi.aabbMax}.transform(t);
        for (int i = 0; i < numViews; i++)
        {
            if (frustums[i].containsAABB(aabb.min, aabb.max))
                reject = false;
        }

        if (reject)
            return false;

        return true;
    }
}