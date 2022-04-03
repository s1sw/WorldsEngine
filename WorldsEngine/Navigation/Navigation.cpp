#include "Navigation.hpp"
#include "Core/Engine.hpp"
#include "Core/NameComponent.hpp"
#include "Util/TimingUtil.hpp"

#include <entt/entity/registry.hpp>
#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>

#include <Core/MeshManager.hpp>
#include <Core/Log.hpp>
#include <Util/EnumUtil.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <physfs.h>
#include <sys/types.h>

namespace worlds {
    const float CellSize = 0.3f;
    const float CellHeight = 0.2f;

    rcConfig recastConfig{
        .width = 0,
        .height = 0,
        .tileSize = 0,
        .borderSize = 0,
        .cs = CellSize,
        .ch = CellHeight,
        .walkableSlopeAngle = 45.0f,
        .walkableHeight = (int)ceilf(2.0f / CellHeight),
        .walkableClimb = (int)floorf(0.9f / CellHeight),
        .walkableRadius = (int)ceilf(0.6f / CellSize),
        .maxEdgeLen = 40,
        .maxSimplificationError = 1.3f,
        .minRegionArea = 64,
        .mergeRegionArea = 400,
        .maxVertsPerPoly = 6,
        .detailSampleDist = 1.8f,
        .detailSampleMaxError = 0.0f
    };
    
    class CustomRCContext : public rcContext {
    public:
        CustomRCContext() : rcContext(true) {}
        virtual ~CustomRCContext() {}
    protected:
        void doLog(const rcLogCategory category, const char* msg, const int len) override {
            switch (category) {
            case RC_LOG_PROGRESS:
                logVrb("Recast: %s", msg);
                break;
            case RC_LOG_WARNING:
                logWarn("Recast: %s", msg);
                break;
            case RC_LOG_ERROR:
                logErr("Recast: %s", msg);
                break;
            }
        }
    private:
    };

    dtNavMesh* NavigationSystem::navMesh = nullptr;
    dtNavMeshQuery* NavigationSystem::navMeshQuery = nullptr;

    void NavigationSystem::setupFromNavMeshData(uint8_t* data, size_t dataSize) {
        if (data == nullptr || dataSize == 0) return;
        if (navMesh) {
            dtFreeNavMesh(navMesh);
            navMesh = nullptr;
        }

        navMesh = dtAllocNavMesh();

        dtStatus status = navMesh->init(data, dataSize, DT_TILE_FREE_DATA);

        if (dtStatusFailed(status)) {
            logErr("Failed to initialise nav mesh");
            return;
        }

        if (navMeshQuery) {
            dtFreeNavMeshQuery(navMeshQuery);
            navMeshQuery = nullptr;
        }

        navMeshQuery = dtAllocNavMeshQuery();

        status = navMeshQuery->init(navMesh, 2048);

        if (dtStatusFailed(status)) {
            logErr("Failed to initialise nav mesh query");
            return;
        }
    }

    void NavigationSystem::loadNavMeshFromFile(const char* path) {
        PHYSFS_File* file = PHYSFS_openRead(path);
        size_t navmeshSize = PHYSFS_fileLength(file);
        uint8_t* navmeshData = new uint8_t[navmeshSize];

        if (navmeshSize != PHYSFS_readBytes(file, navmeshData, navmeshSize)) {
            logErr("Failed to load navmesh");
        }
        PHYSFS_close(file);
        setupFromNavMeshData(navmeshData, navmeshSize);
    }

    void NavigationSystem::buildNavMesh(entt::registry& registry, uint8_t*& dataOut, size_t& dataSizeOut) {
        dataOut = nullptr;
        dataSizeOut = 0;
        // Find the bounding boxes and poly/vert counts of all the navigation static objects in the scene
        glm::vec3 bbMin{FLT_MAX};
        glm::vec3 bbMax{-FLT_MAX};
        uint32_t numTriangles = 0;
        uint32_t numVertices = 0;

        PerfTimer pt;
        registry.view<Transform, WorldObject>().each([&](Transform& t, WorldObject& o) {
            if (!enumHasFlag(o.staticFlags, StaticFlags::Navigation)) return;

            glm::mat4 tMat = t.getMatrix();
            auto& mesh = MeshManager::loadOrGet(o.mesh);
            numTriangles += mesh.indices.size() / 3;
            numVertices += mesh.vertices.size();

            for (const Vertex& v : mesh.vertices) {
                glm::vec3 transformedPosition = tMat * glm::vec4(v.position, 1.0f);
                bbMin = glm::min(transformedPosition, bbMin);
                bbMax = glm::max(transformedPosition, bbMax);
            }
            });

        if (numVertices == 0) {
            logErr("Nothing marked as navigation static!");
            return;
        }

        // Now convert those to a data format Recast understands
        std::vector<int> recastTriangles;
        std::vector<glm::vec3> recastTriNormals;
        recastTriangles.reserve(numTriangles * 3);
        recastTriNormals.reserve(numTriangles);

        std::vector<glm::vec3> recastVertices;
        recastVertices.reserve(numVertices);

        registry.view<Transform, WorldObject>().each([&](Transform& t, WorldObject& o) {
            if (!enumHasFlag(o.staticFlags, StaticFlags::Navigation)) return;
            auto& mesh = MeshManager::loadOrGet(o.mesh);
            glm::mat4 mat = t.getMatrix();

            for (uint32_t idx : mesh.indices) {
                recastTriangles.push_back(idx + recastVertices.size());
            }

            for (uint32_t i = 0; i < mesh.indices.size() / 3; i++) {
                glm::vec3 faceNormal =
                    mesh.vertices[mesh.indices[i * 3 + 0]].normal +
                    mesh.vertices[mesh.indices[i * 3 + 0]].normal +
                    mesh.vertices[mesh.indices[i * 3 + 0]].normal;
                faceNormal /= 3.0f;
                recastTriNormals.push_back(faceNormal);
            }

            for (const Vertex& v : mesh.vertices) {
                glm::vec3 transformedPosition = mat * glm::vec4(v.position, 1.0f);
                recastVertices.push_back(transformedPosition);
            }

            });
        
        for (int i = 0; i < 3; i++) {
            recastConfig.bmin[i] = bbMin[i];
            recastConfig.bmax[i] = bbMax[i];
        }
        
        CustomRCContext* ctx = new CustomRCContext();

        ctx->resetTimers();
        ctx->startTimer(RC_TIMER_TOTAL);

        rcCalcGridSize(recastConfig.bmin, recastConfig.bmax, recastConfig.cs, &recastConfig.width, &recastConfig.height);

        rcHeightfield* heightfield = rcAllocHeightfield();

        bool lastResult = rcCreateHeightfield(ctx, *heightfield,
            recastConfig.width, recastConfig.height, recastConfig.bmin, recastConfig.bmax, recastConfig.cs, recastConfig.ch);

        if (!lastResult) {
            logErr("Failed to create heightfield");
            return;
        }

        uint8_t* triareas = new uint8_t[numTriangles];
        memset(triareas, 0, numTriangles);

        rcMarkWalkableTriangles(ctx, recastConfig.walkableSlopeAngle,
            (float*)recastVertices.data(), numVertices, recastTriangles.data(), numTriangles, triareas);
        lastResult = rcRasterizeTriangles(ctx, (float*)recastVertices.data(), numVertices, recastTriangles.data(), triareas, numTriangles, *heightfield, recastConfig.walkableClimb);

        if (!lastResult) {
            logErr("Failed to rasterize triangles");
            return;
        }

        delete[] triareas;

        rcFilterLowHangingWalkableObstacles(ctx, recastConfig.walkableClimb, *heightfield);
        rcFilterLedgeSpans(ctx, recastConfig.walkableHeight, recastConfig.walkableClimb, *heightfield);
        rcFilterWalkableLowHeightSpans(ctx, recastConfig.walkableHeight, *heightfield);

        rcCompactHeightfield* compactHeightfield = rcAllocCompactHeightfield();
        lastResult = rcBuildCompactHeightfield(ctx, recastConfig.walkableHeight, recastConfig.walkableClimb, *heightfield, *compactHeightfield);

        if (!lastResult) {
            logErr("Failed to build compact heightfield");
            return;
        }

        rcFreeHeightField(heightfield);

        rcErodeWalkableArea(ctx, recastConfig.walkableRadius, *compactHeightfield);
        rcBuildDistanceField(ctx, *compactHeightfield);
        rcBuildRegions(ctx, *compactHeightfield, recastConfig.borderSize, recastConfig.minRegionArea, recastConfig.mergeRegionArea);

        rcContourSet* contourSet = rcAllocContourSet();
        rcBuildContours(ctx, *compactHeightfield, recastConfig.maxSimplificationError, recastConfig.maxEdgeLen, *contourSet);

        rcPolyMesh* polyMesh = rcAllocPolyMesh();
        rcBuildPolyMesh(ctx, *contourSet, recastConfig.maxVertsPerPoly, *polyMesh);

        rcPolyMeshDetail* polyMeshDetail = rcAllocPolyMeshDetail();
        rcBuildPolyMeshDetail(ctx, *polyMesh,
            *compactHeightfield, recastConfig.detailSampleDist, recastConfig.detailSampleMaxError, *polyMeshDetail);

        rcFreeCompactHeightfield(compactHeightfield);
        rcFreeContourSet(contourSet);
        ctx->stopTimer(RC_TIMER_TOTAL);

        for (int i = 0; i < polyMesh->npolys; i++) {
            polyMesh->flags[i] = 1;
        }

        dtNavMeshCreateParams params{};
        params.verts = polyMesh->verts;
        params.vertCount = polyMesh->nverts;
        params.polys = polyMesh->polys;
        params.polyAreas = polyMesh->areas;
        params.polyFlags = polyMesh->flags;
        params.polyCount = polyMesh->npolys;
        params.nvp = polyMesh->nvp;

        params.detailMeshes = polyMeshDetail->meshes;
        params.detailVerts = polyMeshDetail->verts;
        params.detailVertsCount = polyMeshDetail->nverts;
        params.detailTris = polyMeshDetail->tris;
        params.detailTriCount = polyMeshDetail->ntris;

        params.walkableHeight = recastConfig.walkableHeight * recastConfig.ch;
        params.walkableRadius = recastConfig.walkableRadius * recastConfig.cs;
        params.walkableClimb = recastConfig.walkableClimb * recastConfig.ch;
        
        for (int i = 0; i < 3; i++) {
            params.bmin[i] = recastConfig.bmin[i];
            params.bmax[i] = recastConfig.bmax[i];
        }

        params.cs = recastConfig.cs;
        params.ch = recastConfig.ch;
        params.buildBvTree = true;

        uint8_t* navMeshData;
        int navMeshDataSize;
        if (!dtCreateNavMeshData(&params, &navMeshData, &navMeshDataSize)) {
            logErr("Failed to create nav mesh data");
            return;
        }

        rcFreePolyMesh(polyMesh);
        rcFreePolyMeshDetail(polyMeshDetail);
        logMsg("navmesh generation took %.3fms", pt.stopGetMs());
        dataOut = navMeshData;
        dataSizeOut = static_cast<size_t>(navMeshDataSize);
    }

    void NavigationSystem::buildAndSave(entt::registry &registry, const char *path) {
        uint8_t* data;
        size_t dataSize;
        buildNavMesh(registry, data, dataSize);

        PHYSFS_File* file = PHYSFS_openWrite(path);

        if (file == nullptr) {
            logErr("Failed to open writing location for navmesh %s", path);
            return;
        }

        PHYSFS_writeBytes(file, data, dataSize);
        PHYSFS_close(file);
        free(data);
    }

    void NavigationSystem::updateNavMesh(entt::registry& reg) {
        std::string savedPath = "LevelData/Navmeshes/" + reg.ctx<SceneInfo>().name + ".bin";
        
        if (PHYSFS_exists(savedPath.c_str())) {
            loadNavMeshFromFile(savedPath.c_str());
            return;
        }
        logWarn("Scene %s doesn't have baked navmesh data", reg.ctx<SceneInfo>().name.c_str());

        uint8_t* navMeshData;
        size_t navMeshDataSize;
        buildNavMesh(reg, navMeshData, navMeshDataSize);
        setupFromNavMeshData(navMeshData, navMeshDataSize);
    }

    void NavigationSystem::findPath(glm::vec3 startPos, glm::vec3 endPos, NavigationPath& path) {
        path.valid = false;
        if (navMeshQuery == nullptr) return;

        glm::vec3 polySearchExtent{ 1.0f, 3.0f, 1.0f };

        dtQueryFilter queryFilter;

        glm::vec3 onPolyStartPos;
        dtPolyRef startPolygon;

        dtStatus status;
        status = navMeshQuery->findNearestPoly(glm::value_ptr(startPos), glm::value_ptr(polySearchExtent), &queryFilter, &startPolygon, glm::value_ptr(onPolyStartPos));

        if (dtStatusFailed(status)) {
            return;
        }

        glm::vec3 onPolyEndPos;
        dtPolyRef endPolygon;

        status = navMeshQuery->findNearestPoly(glm::value_ptr(endPos), glm::value_ptr(polySearchExtent), &queryFilter, &endPolygon, glm::value_ptr(onPolyEndPos));

        if (dtStatusFailed(status)) {
            return;
        }

        dtPolyRef pathPolys[32];
        int numPathPolys;
        status = navMeshQuery->findPath(startPolygon, endPolygon, glm::value_ptr(onPolyStartPos), glm::value_ptr(onPolyEndPos),
            &queryFilter, pathPolys, &numPathPolys, 32);

        if (dtStatusFailed(status) && !dtStatusDetail(status, DT_PARTIAL_RESULT)) {
            return;
        }

        glm::vec3 pathPoints[32];
        dtPolyRef straightPathPolys[32];
        uint8_t pathFlags[32];
        int numPathPoints;
        status = navMeshQuery->findStraightPath(glm::value_ptr(onPolyStartPos), glm::value_ptr(onPolyEndPos), pathPolys, numPathPolys, glm::value_ptr(pathPoints[0]), pathFlags, straightPathPolys, &numPathPoints, 32, DT_STRAIGHTPATH_ALL_CROSSINGS);

        if (dtStatusFailed(status) && !dtStatusDetail(status, DT_PARTIAL_RESULT)) {
            return;
        }

        path.valid = true;
        path.numPoints = numPathPoints;
        for (int i = 0; i < numPathPoints; i++) {
            path.pathPoints[i] = pathPoints[i];
        }
    }

    bool NavigationSystem::getClosestPointOnMesh(glm::vec3 point, glm::vec3& outPoint, glm::vec3 searchExtent) {
        if (glm::length2(searchExtent) == 0.0f) {
            searchExtent = glm::vec3(1.0f, 3.0f, 1.0f);
        }

        dtQueryFilter queryFilter;
        dtPolyRef startPolygon;

        dtStatus status = navMeshQuery->findNearestPoly(glm::value_ptr(point), glm::value_ptr(searchExtent), &queryFilter, &startPolygon, glm::value_ptr(outPoint));

        if (dtStatusFailed(status)) {
            return false;
        }

        return true;
    }
}
