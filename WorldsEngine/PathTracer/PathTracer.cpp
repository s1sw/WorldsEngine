#include "PathTracer.hpp"
#include "Core/MeshManager.hpp"
#include "Core/WorldComponents.hpp"
#include "glm/ext/quaternion_geometric.hpp"
#include <Core/Console.hpp>
#include <Core/JobSystem.hpp>
#include <Core/Log.hpp>
#include <cfloat>
#include <cmath>
#include <embree3/rtcore_buffer.h>
#include <embree3/rtcore_common.h>
#include <embree3/rtcore_device.h>
#include <embree3/rtcore_geometry.h>
#include <embree3/rtcore_ray.h>
#include <embree3/rtcore_scene.h>
#include <entt/entt.hpp>
#include <random>
#include <stb_image_write.h>

namespace worlds
{
    namespace tonemap
    {
        float A = 0.15f;
        float B = 0.50f;
        float C = 0.10f;
        float D = 0.20f;
        float E = 0.02f;
        float F = 0.30f;
        float W = 11.2f;

        glm::vec3 Uncharted2Tonemap(glm::vec3 x)
        {
            return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
        }
    }
    const int IMAGE_WIDTH = 800;
    const int IMAGE_HEIGHT = 600;

    struct GeomData
    {
        glm::mat4 transform;
        const LoadedMesh &lm;
    };

    PathTracer::PathTracer(entt::registry &reg) : reg(reg), scene(nullptr)
    {
        device = rtcNewDevice(nullptr);
    }

    PathTracer::~PathTracer()
    {
        // Clean up stuff
        if (scene)
            rtcReleaseScene(scene);
        rtcReleaseDevice(device);
    }

    void PathTracer::buildAS()
    {
        if (scene)
        {
            rtcReleaseScene(scene);
        }

        scene = rtcNewScene(device);

        reg.view<WorldObject, Transform>().each([&](WorldObject &wo, Transform &t) {
            RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
            glm::mat4 tMat = t.getMatrix();

            const LoadedMesh &lm = MeshManager::loadOrGet(wo.mesh);
            GeomData *gd = new GeomData{tMat, lm};
            rtcSetGeometryUserData(geom, (void *)gd);

            float *verts = (float *)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
                                                            3 * sizeof(float), lm.vertices.size());

            uint32_t *indices = (uint32_t *)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
                                                                    3 * sizeof(uint32_t), lm.indices.size() / 3);

            for (uint32_t i = 0; i < lm.vertices.size(); i++)
            {
                glm::vec3 pos = tMat * glm::vec4(lm.vertices[i].position, 1.0f);
                verts[i * 3 + 0] = pos.x;
                verts[i * 3 + 1] = pos.y;
                verts[i * 3 + 2] = pos.z;
            }

            memcpy(indices, lm.indices.data(), lm.indices.size() * sizeof(uint32_t));

            rtcCommitGeometry(geom);
            rtcAttachGeometry(scene, geom);
            rtcReleaseGeometry(geom);
        });

        rtcCommitScene(scene);
    }

    bool PathTracer::traceRay(glm::vec3 pos, glm::vec3 dir, RTCRayHit &rayHit)
    {
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

    glm::vec3 uniformSampleHemisphere(float r1, float r2)
    {
        // cos(theta) = u1 = y
        // cos^2(theta) + sin^2(theta) = 1 -> sin(theta) = srtf(1 - cos^2(theta))
        float sinTheta = sqrtf(1 - r1 * r1);
        float phi = 2 * M_PI * r2;
        float x = sinTheta * cosf(phi);
        float z = sinTheta * sinf(phi);
        return glm::vec3(x, r1, z);
    }

    void buildCoordinateSystem(glm::vec3 normal, glm::vec3 &t, glm::vec3 &b)
    {
        if (glm::abs(normal.x) > glm::abs(normal.y))
        {
            t = glm::vec3(normal.z, 0.0f, -normal.x) / glm::sqrt(normal.x * normal.x + normal.z * normal.z);
        }
        else
        {
            t = glm::vec3(0.0f, -normal.z, normal.y) / glm::sqrt(normal.y * normal.y + normal.z * normal.z);
        }

        b = glm::cross(normal, t);
    }

    float calculateFalloff(float pixelDist, float cutoffDist)
    {
        return glm::max((1.0f / (pixelDist * pixelDist)) * (1.0f - (pixelDist / cutoffDist)), 0.0f);
    }

    const int MAX_PT_DEPTH = 2;

    std::default_random_engine generator;
    std::uniform_real_distribution<float> distribution{0.0f, 1.0f};

    glm::vec3 ptrToCol(const PathTraceResult &ptr)
    {
        return ptr.indirect + ptr.direct;
    }

    ConVar numIndirectSamples{"pt_numIndirectSamples", "4"};

    PathTraceResult PathTracer::pathTrace(glm::vec3 pos, glm::vec3 dir, int depth)
    {
        if (depth > MAX_PT_DEPTH)
            return PathTraceResult{};

        RTCRayHit rayHit;
        PathTraceResult result{};
        bool hitAnything = traceRay(pos, dir, rayHit);

        if (hitAnything)
        {
            RTCHit &hit = rayHit.hit;
            RTCGeometry geom = rtcGetGeometry(scene, rayHit.hit.geomID);
            GeomData *gd = reinterpret_cast<GeomData *>(rtcGetGeometryUserData(geom));
            const LoadedMesh &lm = gd->lm;

            const Vertex &v0 = lm.vertices[lm.indices[hit.primID * 3 + 0]];
            const Vertex &v1 = lm.vertices[lm.indices[hit.primID * 3 + 1]];
            const Vertex &v2 = lm.vertices[lm.indices[hit.primID * 3 + 2]];
            glm::vec3 norm = glm::normalize(v1.normal * hit.u + v2.normal * hit.v + v0.normal * (1.0f - hit.v - hit.u));

            glm::vec3 pos;
            rtcInterpolate0(geom, rayHit.hit.primID, rayHit.hit.u, rayHit.hit.v, RTC_BUFFER_TYPE_VERTEX, 0, &pos.x, 3);

            glm::vec3 albedo{0.8f};

            const float bias = 0.0005f;
            reg.view<WorldLight, Transform>().each([&](WorldLight &wl, Transform &t) {
                if (!wl.enabled)
                    return;
                if (wl.type == LightType::Directional)
                {
                    glm::vec3 forward = t.transformDirection(glm::vec3(0.0f, 0.0f, 1.0f));

                    RTCRayHit shadowHit;
                    bool obscured = traceRay(pos + norm * bias, -forward, shadowHit);

                    result.direct += glm::pow(wl.color, glm::vec3(2.2)) * wl.intensity * (obscured ? 0.0f : 1.0f) *
                                     glm::max(glm::dot(norm, -forward), 0.0f) * albedo;
                }
                else if (wl.type == LightType::Spot)
                {
                    glm::vec3 forward = t.transformDirection(glm::vec3(0.0f, 0.0f, -1.0f));
                    glm::vec3 vecToLight = t.position - pos;
                    glm::vec3 dirToLight = glm::normalize(vecToLight);

                    float theta = glm::dot(dirToLight, forward);
                    float spotScalar = glm::clamp((theta - glm::cos(wl.spotCutoff) - 0.02f) / 0.02f, 0.0f, 1.0f);

                    float falloffScalar = calculateFalloff(glm::length(vecToLight), wl.maxDistance);

                    RTCRayHit shadowHit;
                    bool obscured = traceRay(pos + norm * bias, dirToLight, shadowHit);
                    obscured &= shadowHit.ray.tfar < glm::length(dirToLight);
                    float shadowFac = obscured ? 0.0f : 1.0f;
                    float cosLi = glm::max(glm::dot(norm, dirToLight), 0.0f);

                    // result.direct += wl.color * wl.intensity * cosLi * spotScalar * falloffScalar * albedo;
                    result.direct += glm::pow(wl.color, glm::vec3(2.2)) * wl.intensity * cosLi * spotScalar *
                                     falloffScalar * albedo * shadowFac;
                }
            });

            if (numIndirectSamples.getInt() > 0)
            {
                float pdf = 1.0f / (2.0f * glm::pi<float>());
                glm::vec3 t, b;
                buildCoordinateSystem(norm, t, b);
                glm::vec3 indirect{0.0f};
                for (int i = 0; i < numIndirectSamples.getInt(); i++)
                {
                    float r1 = distribution(generator);
                    float r2 = distribution(generator);

                    glm::vec3 localDir = uniformSampleHemisphere(r1, r2);
                    glm::vec3 dir{
                        localDir.x * b.x + localDir.y * norm.x + localDir.z * t.x,
                        localDir.x * b.y + localDir.y * norm.y + localDir.z * t.y,
                        localDir.x * b.z + localDir.y * norm.z + localDir.z * t.z,
                    };
                    PathTraceResult indirectResult = pathTrace(pos + dir * bias, dir, depth + 1);

                    glm::vec3 overallResult = ptrToCol(indirectResult);
                    indirect += r1 * overallResult * albedo;
                }
                indirect /= (float)numIndirectSamples.getFloat();
                result.indirect = indirect;
            }
        }
        else
        {
            result.direct = glm::vec3(0.1f);
            result.indirect = glm::vec3(0.0f);
        }

        return result;
    }

    struct RenderTile
    {
        int xMin;
        int yMin;
        int xMax;
        int yMax;
        Camera &cam;
        uint8_t *outputBuffer;
        float imageAspectRatio;
    };

    void PathTracer::traceTile(RenderTile &rt)
    {
        glm::vec3 whiteScale = 1.0f / tonemap::Uncharted2Tonemap(glm::vec3(tonemap::W));
        Camera &cam = rt.cam;
        float exposure = g_console->getConVar("r_exposure")->getFloat();

        for (int x = rt.xMin; x < rt.xMax; x++)
        {
            for (int y = rt.yMin; y < rt.yMax; y++)
            {
                float Px = -(2 * ((x + 0.5f) / IMAGE_WIDTH) - 1) * tan(cam.verticalFOV * 0.5f) * rt.imageAspectRatio;
                float Py = (1 - 2 * ((y + 0.5f) / IMAGE_HEIGHT)) * tan(cam.verticalFOV * 0.5f);
                glm::vec3 dir{Px, Py, 1.0f};
                dir = cam.rotation * glm::normalize(dir);

                PathTraceResult ptr = pathTrace(cam.position, dir);

                glm::vec3 col = ptr.direct / glm::pi<float>() + 2.0f * ptr.indirect;
                col = tonemap::Uncharted2Tonemap(16.0f * exposure * col) * whiteScale;
                col = glm::pow(col, glm::vec3(1.0f / 2.2f));
                col = glm::min(col, glm::vec3(1.0f));

                rt.outputBuffer[(y * IMAGE_WIDTH + x) * 3 + 0] = col.x * 255;
                rt.outputBuffer[(y * IMAGE_WIDTH + x) * 3 + 1] = col.y * 255;
                rt.outputBuffer[(y * IMAGE_WIDTH + x) * 3 + 2] = col.z * 255;
            }
        }
    }

    const int TILE_SIZE = 16;
    void PathTracer::trace(Camera cam, slib::String imagePath)
    {
        uint8_t *outputBuffer = new uint8_t[IMAGE_WIDTH * IMAGE_HEIGHT * 3];
        float imageAspectRatio = (float)IMAGE_WIDTH / IMAGE_HEIGHT; // assuming width > height

        glm::vec3 whiteScale = 1.0f / tonemap::Uncharted2Tonemap(glm::vec3(tonemap::W));

        float exposure = g_console->getConVar("r_exposure")->getFloat();

        JobList &jl = g_jobSys->getFreeJobList();
        jl.begin();
        int numXTiles = ((IMAGE_WIDTH + TILE_SIZE - 1) / TILE_SIZE);
        int numYTiles = ((IMAGE_HEIGHT + TILE_SIZE - 1) / TILE_SIZE);
        int numTiles = numXTiles * numYTiles;

        for (int x = 0; x < numXTiles; x++)
        {
            for (int y = 0; y < numYTiles; y++)
            {
                Job j{[&, x, y] {
                    RenderTile rt{.cam = cam, .outputBuffer = outputBuffer};
                    rt.xMin = x * TILE_SIZE;
                    rt.xMax = (x + 1) * TILE_SIZE;
                    rt.xMax = glm::min(rt.xMax, IMAGE_WIDTH);

                    rt.yMin = y * TILE_SIZE;
                    rt.yMax = (y + 1) * TILE_SIZE;
                    rt.yMax = glm::min(rt.yMax, IMAGE_HEIGHT);
                    rt.imageAspectRatio = imageAspectRatio;
                    traceTile(rt);
                }};
                // j.function();
                jl.addJob(std::move(j));
            }
        }
        jl.end();
        g_jobSys->signalJobListAvailable();
        jl.wait();

        stbi_write_png("trace.png", IMAGE_WIDTH, IMAGE_HEIGHT, 3, outputBuffer, 0);

        delete[] outputBuffer;
    }
}