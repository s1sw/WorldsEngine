#include "PathTracer.hpp"
#include "Core/MeshManager.hpp"
#include "Core/WorldComponents.hpp"
#include "glm/ext/quaternion_geometric.hpp"
#include <cfloat>
#include <cmath>
#include <embree3/rtcore_buffer.h>
#include <embree3/rtcore_common.h>
#include <embree3/rtcore_device.h>
#include <embree3/rtcore_geometry.h>
#include <embree3/rtcore_ray.h>
#include <embree3/rtcore_scene.h>
#include <entt/entt.hpp>
#include <stb_image_write.h>

namespace worlds {
    const int IMAGE_WIDTH = 800;
    const int IMAGE_HEIGHT = 600;

    PathTracer::PathTracer(entt::registry& reg) : reg(reg), scene(nullptr) {
        device = rtcNewDevice(nullptr);
    }

    PathTracer::~PathTracer() {
        // Clean up stuff
        if (scene)
            rtcReleaseScene(scene);
        rtcReleaseDevice(device);
    }

    void PathTracer::buildAS() {
        if (scene) {
            rtcReleaseScene(scene);
        }

        scene = rtcNewScene(device);

        reg.view<WorldObject, Transform>().each([&](WorldObject& wo, Transform& t) {
            RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
            glm::mat4 tMat = t.getMatrix();

            const LoadedMesh& lm = MeshManager::loadOrGet(wo.mesh);

            rtcSetGeometryVertexAttributeCount(geom, 1);
            float* verts = (float*)rtcSetNewGeometryBuffer(
                geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
                3 * sizeof(float), lm.vertices.size()
            );

            float* normals = (float*)rtcSetNewGeometryBuffer(
                geom, RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 0, RTC_FORMAT_FLOAT3,
                3 * sizeof(float), lm.vertices.size());

            uint32_t* indices = (uint32_t*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(uint32_t), lm.indices.size() / 3);

            for (uint32_t i = 0; i < lm.vertices.size(); i++) {
                glm::vec3 pos = tMat * glm::vec4(lm.vertices[i].position, 1.0f);
                verts[i * 3 + 0] = pos.x;
                verts[i * 3 + 1] = pos.y;
                verts[i * 3 + 2] = pos.z;
                glm::vec3 norm = glm::transpose(glm::inverse(tMat)) * glm::vec4(lm.vertices[i].normal, 0.0f);
                normals[i * 3 + 0] = norm.x;
                normals[i * 3 + 1] = norm.y;
                normals[i * 3 + 2] = norm.z;
            }

            memcpy(indices, lm.indices.data(), lm.indices.size() * sizeof(uint32_t));

            rtcCommitGeometry(geom);
            rtcAttachGeometry(scene, geom);
            rtcReleaseGeometry(geom);
        });

        rtcCommitScene(scene);
    }

    bool PathTracer::traceRay(glm::vec3 pos, glm::vec3 dir, RTCRayHit& rayHit) {
        RTCRay ray;
        ray.org_x = pos.x;
        ray.org_y = pos.y;
        ray.org_z = pos.z;
        ray.dir_x = dir.x;
        ray.dir_y = dir.y;
        ray.dir_z = dir.z;
        ray.tnear = 0.0f;
        ray.tfar = INFINITY;

        rayHit.ray = ray;
        rayHit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        rtcIntersect1(scene, &context, &rayHit);

        return rayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID;
    }

    PathTraceResult PathTracer::pathTrace(glm::vec3 pos, glm::vec3 dir) {

    }

    void PathTracer::trace(Camera cam, slib::String imagePath) {
        uint8_t* outputBuffer = new uint8_t[IMAGE_WIDTH * IMAGE_HEIGHT * 3];
        float imageAspectRatio = (float)IMAGE_WIDTH / IMAGE_HEIGHT; // assuming width > height 

        for (int x = 0; x < IMAGE_WIDTH; x++) {
            for (int y = 0; y < IMAGE_HEIGHT; y++) {
                float Px = -(2 * ((x + 0.5) / IMAGE_WIDTH) - 1) * tan(cam.verticalFOV * 0.5f) * imageAspectRatio; 
                float Py = (1 - 2 * ((y + 0.5) / IMAGE_HEIGHT)) * tan(cam.verticalFOV * 0.5f); 
                glm::vec3 dir{Px, Py, 1.0f};
                dir = cam.rotation * glm::normalize(dir);

                RTCRayHit rayHit;
                traceRay(cam.position, dir, rayHit);

                if (rayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
                    RTCGeometry geom = rtcGetGeometry(scene, rayHit.hit.geomID);
                    glm::vec3 norm;
                    rtcInterpolate0(geom, rayHit.hit.primID, rayHit.hit.u, rayHit.hit.u, RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 0, &norm.x, 3);
                    norm = glm::normalize(norm);

                    glm::vec3 pos;
                    rtcInterpolate0(geom, rayHit.hit.primID, rayHit.hit.u, rayHit.hit.v, RTC_BUFFER_TYPE_VERTEX, 0, &pos.x, 3);

                    RTCRayHit shadowHit;
                    bool didShadowHit = traceRay(pos + norm * FLT_EPSILON, glm::vec3(0.0f, 1.0f, 0.0f), shadowHit);

                    outputBuffer[(y * IMAGE_WIDTH + x) * 3 + 0] = didShadowHit ? 127 : 255;
                    outputBuffer[(y * IMAGE_WIDTH + x) * 3 + 1] = didShadowHit ? 127 : 255;
                    outputBuffer[(y * IMAGE_WIDTH + x) * 3 + 2] = didShadowHit ? 127 : 255;
                } else {
                    outputBuffer[(y * IMAGE_WIDTH + x) * 3 + 0] = 0;
                    outputBuffer[(y * IMAGE_WIDTH + x) * 3 + 1] = 0;
                    outputBuffer[(y * IMAGE_WIDTH + x) * 3 + 2] = 0;
                }
            }
        }

        stbi_write_png("trace.png", IMAGE_WIDTH, IMAGE_HEIGHT, 3, outputBuffer, 0);

        delete[] outputBuffer;
    }
}