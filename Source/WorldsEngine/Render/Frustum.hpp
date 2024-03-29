#pragma once
#include <Core/Log.hpp>
#include <Util/AABB.hpp>
#include <glm/glm.hpp>

namespace worlds
{
    struct Plane
    {
        Plane(glm::vec4 v4) : a(v4.x), b(v4.y), c(v4.z), d(v4.w)
        {
        }
        Plane()
        {
        }
        float a, b, c, d;

        void normalize()
        {
            float len = glm::sqrt((a * a) + (b * b) + (c * c));

            a /= len;
            b /= len;
            c /= len;
            d /= len;
        }

        operator glm::vec4()
        {
            return glm::vec4{a, b, c, d};
        }

        glm::vec3 normal() const
        {
            return glm::vec3{a, b, c};
        }

        float pointDistance(glm::vec3 point)
        {
            return glm::dot(point, normal()) + d;
        }
    };

    class Frustum
    {
      public:
        enum FrustumPlane
        {
            Left,
            Right,
            Bottom,
            Top,
            Near,
            Far,
            Count
        };

        Plane planes[FrustumPlane::Count];
        glm::vec3 points[8];

        void fromVPMatrix(glm::mat4 vp)
        {
            glm::mat4 tVP = glm::transpose(vp);
            Plane& left = planes[FrustumPlane::Left];
            left = Plane{tVP[3] + tVP[0]};

            Plane& right = planes[FrustumPlane::Right];
            right = Plane{tVP[3] - tVP[0]};

            Plane& bottom = planes[FrustumPlane::Bottom];
            bottom = Plane{tVP[3] + tVP[1]};

            Plane& top = planes[FrustumPlane::Top];
            top = Plane{tVP[3] - tVP[1]};

            Plane& near = planes[FrustumPlane::Near];
            near = Plane{tVP[3] + tVP[2]};

            Plane& far = planes[FrustumPlane::Far];
            far = Plane{tVP[3] - tVP[2]};

            for (int i = 0; i < 6; i++)
            {
                planes[i].normalize();
            }

            glm::mat4 invVP = glm::inverse(vp);

            glm::vec2 ndcScreenPoints[4]{glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(1.0f, 1.0f),
                                         glm::vec2(-1.0f, 1.0f)};

            const float NEAR_EPSILON = 1e-4;
            // near plane points
            for (int i = 0; i < 4; i++)
            {
                glm::vec4 projected = invVP * glm::vec4(ndcScreenPoints[i], NEAR_EPSILON, 1.0f);
                points[i] = glm::vec3(projected) / projected.w;
            }

            // far plane points
            for (int i = 0; i < 4; i++)
            {
                glm::vec4 projected = invVP * glm::vec4(ndcScreenPoints[i], 1.0f, 1.0f);
                points[i + 4] = glm::vec3(projected) / projected.w;
            }
        }

        bool containsSphere(glm::vec3 center, float radius)
        {
            for (int i = 0; i < 6; i++)
            {
                const Plane& plane = planes[i];
                float distance = glm::dot(center, plane.normal()) + plane.d;
                if (distance < -radius)
                    return false;
            }

            return true;
        }

        bool containsAABB(glm::vec3 min, glm::vec3 max)
        {
            glm::vec3 points[] = {min,
                                  glm::vec3(max.x, min.y, min.z),
                                  glm::vec3(min.x, max.y, min.z),
                                  glm::vec3(max.x, max.y, min.z),
                                  glm::vec3(min.x, min.y, max.z),
                                  glm::vec3(max.x, min.y, max.z),
                                  glm::vec3(min.x, max.y, max.z),
                                  glm::vec3(max.x, max.y, max.z)};

            for (int i = 0; i < 6; i++)
            {
                bool inside = false;

                for (int j = 0; j < 8; j++)
                {
                    if (planes[i].pointDistance(points[j]) > 0.0f)
                    {
                        inside = true;
                        break;
                    }
                }

                if (!inside)
                    return false;
            }

            int outside[6] = {0};
            for (int i = 0; i < 8; i++)
            {
                // on each axis...
                for (int j = 0; j < 3; j++)
                {
                    outside[j] += points[i][j] > max[j];
                    outside[j + 3] += points[i][j] < min[j];
                }
            }

            for (int i = 0; i < 6; i++)
            {
                if (outside[i] == 8)
                    return false;
            }

            return true;
        }
    };
}
