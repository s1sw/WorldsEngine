#pragma once
#include <Core/Transform.hpp>
#include <glm/vec3.hpp>

namespace worlds
{
    struct AABB
    {
        AABB() = default;
        AABB(glm::vec3 min, glm::vec3 max) : min(min), max(max)
        {
        }

        glm::vec3 min;
        glm::vec3 max;

        bool containsPoint(glm::vec3 point)
        {
            return point.x > min.x && point.x < max.x && point.y > min.y && point.y < max.y && point.z > min.z &&
                   point.z < max.z;
        }

        AABB transform(const Transform& t)
        {
            glm::vec3 aabbMin{FLT_MAX};
            glm::vec3 aabbMax{-FLT_MAX};

            glm::vec3 mi = min * t.scale;
            glm::vec3 ma = max * t.scale;

            glm::vec3 points[] = 
            {
                mi,
                glm::vec3(ma.x, mi.y, mi.z),
                glm::vec3(mi.x, ma.y, mi.z),
                glm::vec3(ma.x, ma.y, mi.z),
                glm::vec3(mi.x, mi.y, ma.z),
                glm::vec3(ma.x, mi.y, ma.z),
                glm::vec3(mi.x, ma.y, ma.z),
                glm::vec3(ma.x, ma.y, ma.z)
            };

            for (int i = 0; i < 8; i++)
            {
                glm::vec3 p = t.transformPoint(points[i]);
                aabbMin = glm::min(aabbMin, p);
                aabbMax = glm::max(aabbMax, p);
            }

            return AABB { aabbMin, aabbMax };
        }

        glm::vec3 center()
        {
            return (min + max) * 0.5f;
        }

        glm::vec3 extents()
        {
            return max - min;
        }

        bool intersects(AABB& other)
        {
            bool isects = true;

            for (int i = 0; i < 3; i++)
            {
                isects &= min[i] <= other.max[i] && max[i] >= other.min[i];
            }

            return isects;
        }
    };
}