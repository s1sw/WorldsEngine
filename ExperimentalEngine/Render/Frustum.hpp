#pragma once
#include <glm/glm.hpp>

namespace worlds {
    struct Plane {
        Plane(glm::vec4 v4) : a(v4.x), b(v4.y), c(v4.z), d(v4.w) {}
        Plane() {}
        float a, b, c, d;

        void normalize() {
            float len = glm::sqrt((a * a) + (b * b) + (c * c));

            a /= len;
            b /= len;
            c /= len;
            d /= len;
        }

        operator glm::vec4() {
            return glm::vec4{ a, b, c, d };
        }

        glm::vec3 normal() {
            return glm::vec3{ a, b, c };
        }
    };

    class Frustum {
    public:
        enum FrustumPlane {
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

        void fromVPMatrix(glm::mat4 vp) {
            glm::mat4 tVP = glm::transpose(vp);
            Plane& left = planes[FrustumPlane::Left];
            left = Plane{ tVP[3] + tVP[0] };

            Plane& right = planes[FrustumPlane::Right];
            right = Plane{ tVP[3] - tVP[0] };

            Plane& bottom = planes[FrustumPlane::Bottom];
            bottom = Plane{ tVP[3] + tVP[1] };

            Plane& top = planes[FrustumPlane::Top];
            top = Plane{ tVP[3] - tVP[1] };

            Plane& near = planes[FrustumPlane::Near];
            near = Plane{ tVP[3] + tVP[2] };

            Plane& far = planes[FrustumPlane::Far];
            far = Plane{ tVP[3] - tVP[2] };

            for (int i = 0; i < 6; i++) {
                planes[i].normalize();
            }

            glm::mat4 invVP = glm::inverse(vp);

            // near plane points
            points[0] = glm::vec3(invVP * glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f));
            points[1] = glm::vec3(invVP * glm::vec4(1.0f, -1.0f, 0.0f, 1.0f));
            points[2] = glm::vec3(invVP * glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
            points[3] = glm::vec3(invVP * glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f));

            // far plane points
            points[4] = glm::vec3(invVP * glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f));
            points[5] = glm::vec3(invVP * glm::vec4(1.0f, -1.0f, 1.0f, 1.0f));
            points[6] = glm::vec3(invVP * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            points[7] = glm::vec3(invVP * glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f));
        }

        bool containsSphere(glm::vec3 center, float radius) {
            for (int i = 0; i < 6; i++) {
                Plane plane = planes[i];
                float distance = glm::dot(center, plane.normal()) + plane.d;
                if (distance < -radius)
                    return false;
            }
            return true;
        }

        bool containsAABB(glm::vec3 min, glm::vec3 max) {
            for (int i = 0; i < 6; i++) {
                int out = 0;

                out += ((glm::dot((glm::vec4)planes[i], glm::vec4(min.x, min.y, min.z, 1.0f)) < 0.0f) ? 1 : 0);
                out += ((glm::dot((glm::vec4)planes[i], glm::vec4(max.x, min.y, min.z, 1.0f)) < 0.0f) ? 1 : 0);
                out += ((glm::dot((glm::vec4)planes[i], glm::vec4(min.x, max.y, min.z, 1.0f)) < 0.0f) ? 1 : 0);
                out += ((glm::dot((glm::vec4)planes[i], glm::vec4(max.x, max.y, min.z, 1.0f)) < 0.0f) ? 1 : 0);
                out += ((glm::dot((glm::vec4)planes[i], glm::vec4(min.x, min.y, max.z, 1.0f)) < 0.0f) ? 1 : 0);
                out += ((glm::dot((glm::vec4)planes[i], glm::vec4(max.x, min.y, max.z, 1.0f)) < 0.0f) ? 1 : 0);
                out += ((glm::dot((glm::vec4)planes[i], glm::vec4(min.x, max.y, max.z, 1.0f)) < 0.0f) ? 1 : 0);
                out += ((glm::dot((glm::vec4)planes[i], glm::vec4(max.x, max.y, max.z, 1.0f)) < 0.0f) ? 1 : 0);

                if (out == 8)
                    return false;
            }

            int out;
            out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x > max.x) ? 1 : 0); if (out == 8) return false;
            out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x < min.x) ? 1 : 0); if (out == 8) return false;
            out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y > max.y) ? 1 : 0); if (out == 8) return false;
            out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y < min.y) ? 1 : 0); if (out == 8) return false;
            out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z > max.z) ? 1 : 0); if (out == 8) return false;
            out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z < min.z) ? 1 : 0); if (out == 8) return false;

            return true;
        }
    };
}