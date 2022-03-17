#pragma once
#include <Render/Camera.hpp>
#include <embree3/rtcore_ray.h>
#include <slib/String.hpp>
#include <entt/entity/lw_fwd.hpp>
#include <embree3/rtcore.h>

namespace worlds {
    struct PathTraceResult {
        glm::vec3 direct;
        glm::vec3 indirect;
    };

    class PathTracer {
    private:
        entt::registry& reg;
        RTCDevice device;
        RTCScene scene;
        bool traceRay(glm::vec3 pos, glm::vec3 dir, RTCRayHit& rayHit);
        PathTraceResult pathTrace(glm::vec3 pos, glm::vec3 dir);
    public:
        PathTracer(entt::registry& reg);
        ~PathTracer();
        void buildAS();
        void trace(Camera cam, slib::String imagePath);
    };
}