import Light;
import AOBox;
import AOSphere;
import Cubemap;

struct LightingTile
{
    uint lightIdMasks[8];
    uint cubemapIdMasks[2];
    uint aoBoxIdMasks[2];
    uint aoSphereIdMasks[2];
}

struct LightTileInfo
{
    uint tileSize;
    uint tilesPerEye;
    uint numTilesX;
    uint numTilesY;
}

ConstantBuffer<LightTileInfo> buf_LightTileInfo;

struct LightBuffer
{
    // 0
    float4x4 otherShadowMatrices[4];

    // 256
    uint lightCount;
    uint aoBoxCount;
    uint aoSphereCount;
    uint cubemapCount;

    // 272
    float4 cascadeTexelsPerUnit;
    float4x4 dirShadowMatrices[4];

    // 544
    Light lights[256];
    // 12832
    AOBox aoBox[128];
    // 21024
    AOSphere aoSphere[16];
    // 21280
    uint sphereIds[16];
    // 21344
    Cubemap cubemaps[64];
}

ByteAddressBuffer rawLightBuffer;

cbuffer MultiVP
{
    float4x4 view[2];
    float4x4 projection[2];
    float4x4 inverseVP[2];
    float4 viewPos[2];
}

#ifdef MSAA
Sampler2DMSArray depthBuffer;
#else
Sampler2DArray depthBuffer;
#endif
#define DEBUG

RWStructuredBuffer<uint> tileLightCounts;
RWStructuredBuffer<LightingTile> buf_LightTiles;

[[vk::push_constant]]
cbuffer PC
{
    float4x4 invViewProj;
    uint screenWidth;
    uint screenHeight;
    uint eyeIdx;
}

struct Frustum
{
    float4 planes[6];
}

struct AABB
{
    float3 center;
    float3 extents;
}

#define CULL_DEPTH
#define CULL_AABB
#define USE_SUBGROUPS

groupshared Frustum tileFrustum;
groupshared AABB tileAABB;
groupshared uint minDepthU;
groupshared uint maxDepthU;


// HLSL can't load individual fields from a buffer, so we
// have to do alllllll of this to load just what we need
// from a ByteAddressBuffer otherwise performance is terrible
struct LightBufferLoader
{
    static uint GetLightCount()
    {
        return rawLightBuffer.Load(256);
    }

    static Light GetLight(int lightIndex)
    {
        return rawLightBuffer.Load<Light>(544 + LIGHT_SIZE * lightIndex);
    }
    
    static uint GetAOBoxCount()
    {
        return rawLightBuffer.Load(260);
    }

    static AOBox GetAOBox(int boxIndex)
    {
        return rawLightBuffer.Load<AOBox>(12832 + AOBOX_SIZE * boxIndex);
    }

    static uint GetAOSphereCount()
    {
        return rawLightBuffer.Load(264);
    }

    static AOSphere GetAOSphere(int sphereIndex)
    {
        return rawLightBuffer.Load<AOSphere>(21024 + AOSPHERE_SIZE * sphereIndex);
    }

    static uint GetSphereID(int sphereIndex)
    {
        return rawLightBuffer.Load(21280 + sphereIndex * 4);
    }

    static uint GetCubemapCount()
    {
        return rawLightBuffer.Load(268);
    }

    static Cubemap GetCubemap(int cubemapIndex)
    {
        return rawLightBuffer.Load<Cubemap>(21344 + CUBEMAP_SIZE * cubemapIndex);
    }
}

bool frustumContainsSphere(float3 spherePos, float sphereRadius) {
    bool inside = true;
    for (int i = 0; i < 6; i++) {
        float dist = dot(spherePos, tileFrustum.planes[i].xyz) + tileFrustum.planes[i].w;

        if (dist < -sphereRadius)
            inside = false;
    }

    return inside;
}

bool aabbContainsSphere(float3 spherePos, float sphereRadius) {
    float3 vDelta = max(float3(0.0), abs(tileAABB.center - spherePos) - tileAABB.extents);

    float fDistSq = dot(vDelta, vDelta);

    return fDistSq <= sphereRadius * sphereRadius;
}

bool frustumContainsOBB(float3 boxSize, float4x4 transform) {
    // We can determine if the frustum contains an OBB by checking if it contains
    // any vertices of the OBB.

    for (int i = 0; i < 6; i++) {
        int outside = 0;

        for (int j = 0; j < 8; j++) {
            float3 v = float3(j % 2 == 0 ? -1.0 : 1.0, (j >> 1) % 2 == 0 ? -1.0 : 1.0, j < 4 ? -1.0 : 1.0);
            v = mul(transform, float4(v * boxSize, 1.0)).xyz;
            outside += (dot(tileFrustum.planes[i], float4(v, 1.0)) < 0.0) ? 1 : 0;
        }

        if (outside == 8) return false;
    }

    return true;
}

float3 aabbPoint(int index, float3 min, float3 max) {
    return float3(index % 2 == 0 ? min.x : max.x, (index >> 1) % 2 == 0 ? min.y : max.y, index < 4 ? min.z : max.z);
}

bool frustumContainsAABB(float3 min, float3 max) {
    float3 points[8] = { 
        min,
        float3(max.x, min.y, min.z),
        float3(min.x, max.y, min.z),
        float3(max.x, max.y, min.z),
        float3(min.x, min.y, max.z),
        float3(max.x, min.y, max.z),
        float3(min.x, max.y, max.z),
        float3(max.x, max.y, max.z)
    };

    for (int i = 0; i < 6; i++) {
        bool inside = false;

        for (int j = 0; j < 8; j++) {
            if (dot(tileFrustum.planes[i], float4(points[j], 1.0)) > 0.0) {
                inside = true;
                break;
            }
        }

        if (!inside) return false;
    }

    int outside[6] = { 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 8; i++) {
        // on each axis...
        for (int j = 0; j < 3; j++) {
            outside[j] += int(points[i][j] > max[j]);
            outside[j + 3] += int(points[i][j] < min[j]);
        }
    }

    for (int i = 0; i < 6; i++) {
        if (outside[i] == 8) return false;
    }

    return true;
}

bool aabbContainsAABB(float3 min, float3 max) {
    float3 tileMin = tileAABB.center - tileAABB.extents;
    float3 tileMax = tileAABB.center + tileAABB.extents;

    for (int i = 0; i < 3; i++) {
        if (!(tileMin[i] <= max[i] && tileMax[i] >= min[i])) {
            return false;
        }
    }

    return true;
}

float3 getTileMin() {
    return min(tileAABB.center + tileAABB.extents, tileAABB.center - tileAABB.extents);
}

float3 getTileMax() {
    return max(tileAABB.center + tileAABB.extents, tileAABB.center - tileAABB.extents);
}

bool aabbContainsPoint(float3 point) {
    float3 mi = getTileMin();
    float3 ma = getTileMax();

    return
        point.x >= mi.x && point.x <= ma.x &&
        point.y >= mi.y && point.y <= ma.y &&
        point.z >= mi.z && point.z <= ma.z;
}

// Calculates frustum planes from the set of frustum points
void setupTileFrustum(float3 frustumPoints[8]) {
    float3 camPos = viewPos[eyeIdx].xyz;

    for (int i = 0; i < 4; i++) {
        float3 planeNormal = cross(frustumPoints[i] - camPos, frustumPoints[i + 1] - camPos);
        planeNormal = normalize(planeNormal);
        tileFrustum.planes[i] = float4(planeNormal, -dot(planeNormal, frustumPoints[i]));
    }

    // Near plane
    {
        float3 planeNormal = cross(frustumPoints[1] - frustumPoints[0], frustumPoints[3] - frustumPoints[0]);
        planeNormal = normalize(planeNormal);
        tileFrustum.planes[4] = float4(planeNormal, -dot(planeNormal, frustumPoints[0]));
    }

    // Far plane
    {
        float3 planeNormal = cross(frustumPoints[7] - frustumPoints[4], frustumPoints[5] - frustumPoints[4]);
        planeNormal = normalize(planeNormal);
        tileFrustum.planes[5] = float4(planeNormal, -dot(planeNormal, frustumPoints[4]));
    }
}

// Calculates the tile's AABB from the set of frustum points
void setupTileAABB(float3 frustumPoints[8]) {
    // Calculate tile AABB
    float3 aabbMax = float3(0.0f);
    float3 aabbMin = float3(10000000.0f);

    for (int i = 0; i < 8; i++) {
        aabbMax = max(aabbMax, frustumPoints[i]);
        aabbMin = min(aabbMin, frustumPoints[i]);
    }

    tileAABB.center = (aabbMax + aabbMin) * 0.5;
    tileAABB.extents = (aabbMax - aabbMin) * 0.5;
}

void setupTile(uint tileIndex, uint x, uint y) {
    float3 camPos = viewPos[eyeIdx].xyz;
    float minDepth = asfloat(minDepthU);
    float maxDepth = asfloat(maxDepthU);

    tileLightCounts[tileIndex] = 0;
    float tileSize = buf_LightTileInfo.tileSize;
    float2 ndcTileSize = 2.0f * float2(tileSize, -tileSize) / float2(screenWidth, screenHeight);

    // Calculate frustum
    float2 ndcTopLeftCorner = float2(-1.0f, 1.0f);
    float2 tileCoords = float2(x, y);

    float2 ndcTileCorners[4] = {
        ndcTopLeftCorner + ndcTileSize * tileCoords, // Top left
        ndcTopLeftCorner + ndcTileSize * (tileCoords + float2(1, 0)), // Top right
        ndcTopLeftCorner + ndcTileSize * (tileCoords + float2(1, 1)), // Bottom right
        ndcTopLeftCorner + ndcTileSize * (tileCoords + float2(0, 1)), // Bottom left
    };

    float4x4 invVP = inverseVP[eyeIdx];
    float3 frustumPoints[8];

#ifdef CULL_DEPTH
    float nearZ = maxDepth;
    float farZ = minDepth;
#else
    float nearZ = 1.0f;
    float farZ = 0.000001f;
#endif

    // Find the points on the near plane
    for (int i = 0; i < 4; i++) {
        float4 projected = mul(invVP, float4(ndcTileCorners[i], nearZ, 1.0f));
        frustumPoints[i] = projected.xyz / projected.w;
    }

    for (int i = 0; i < 4; i++) {
        float4 projected = mul(invVP, float4(ndcTileCorners[i], farZ, 1.0f));
        frustumPoints[i + 4] = projected.xyz / projected.w;
    }

    setupTileFrustum(frustumPoints);
    setupTileAABB(frustumPoints);
}

void clearBuffers(uint tileIndex, uint groupIndex) {
    if (groupIndex == 0) {
        minDepthU = asuint(1.0);
        maxDepthU = 0u;
    }

    // Clear light values. This is doing a bunch of unnecessary writes, but I'm unsure if
    // that really matters in terms of performance.
    buf_LightTiles[tileIndex].lightIdMasks[groupIndex % 8] = 0u;
    buf_LightTiles[tileIndex].aoSphereIdMasks[groupIndex % 2] = 0u;
    buf_LightTiles[tileIndex].aoBoxIdMasks[groupIndex % 2] = 0u;
    buf_LightTiles[tileIndex].cubemapIdMasks[groupIndex & 2] = 0u;
}

void cullLights(uint tileIndex, uint groupIndex) {
    // Stage 3: Cull lights against the frustum. At this point,
    // one invocation = one light. Since the light array is always
    // tightly packed, we can just check if the index is less than
    // the light count.
    if (groupIndex < LightBufferLoader.GetLightCount()) {
        uint lightIndex = groupIndex;
        Light light = LightBufferLoader.GetLight(lightIndex);
        float3 lightPosition = light.pack2.xyz;

        LightType lightType = light.Type;

        if (lightType == LightType.Tube) {
            // Take the average position of the two end points
            lightPosition = (light.pack1.xyz + light.pack2.xyz) * 0.5;
        }

        float lightRadius = light.DistanceCutoff;

#ifdef CULL_AABB
        bool inFrustum = frustumContainsSphere(lightPosition, lightRadius) && aabbContainsSphere(lightPosition, lightRadius);
#else
        bool inFrustum = frustumContainsSphere(lightPosition, lightRadius);
#endif

        uint bucketIdx = lightIndex / 32;
        uint bucketBit = lightIndex % 32;

        // Subgroup fun: take the maximum bucket ID, perform an or operation
        // along the subgroup's light bitmask and then elect a single invocation
        // to perform the atomic.
        // This results in a *tiny* performance uplift.
#ifdef USE_SUBGROUPS
        uint maxBucketId = WaveActiveMax(bucketIdx);
        for (int i = 0; i <= maxBucketId; i++) {
            if (i == bucketIdx) {
                uint setBits = WaveReadLaneFirst(WaveActiveBitOr(uint(inFrustum || lightType == LightType.Directional) << bucketBit));
                if (WaveIsFirstLane())
                    InterlockedOr(buf_LightTiles[tileIndex].lightIdMasks[i], setBits);
            }
        }
#else
        uint setBits = uint(inFrustum || lightType == LightType.Directional) << bucketBit;
        InterlockedOr(buf_LightTiles[tileIndex].lightIdMasks[bucketIdx], setBits);
#endif

#ifdef DEBUG
        if (inFrustum || lightType == LightType.Directional)
            InterlockedAdd(tileLightCounts[tileIndex], 1);
#endif
    }
}

void cullAO(uint tileIndex, uint groupIndex) {
    // Stage 3: Cull AO spheres against the frustum
    if (groupIndex < LightBufferLoader.GetAOSphereCount()) {
        uint sphereIndex = groupIndex;
        AOSphere sph = LightBufferLoader.GetAOSphere(sphereIndex);
        float3 spherePos = sph.Position;
        float cullRadius = sph.Radius + 1.0;

#ifdef CULL_AABB
        bool inFrustum = frustumContainsSphere(spherePos, cullRadius) && aabbContainsSphere(spherePos, cullRadius);
#else
        bool inFrustum = frustumContainsSphere(spherePos, cullRadius);
#endif

        uint bucketIdx = sphereIndex / 32;
        uint bucketBit = sphereIndex % 32;
        uint maxBucketId = WaveActiveMax(bucketIdx);

        // Subgroup fun again
        for (int i = 0; i <= maxBucketId; i++) {
            if (i == bucketIdx) {
                uint setBits = WaveReadLaneFirst(WaveActiveBitOr(uint(inFrustum) << bucketBit));
                if (WaveIsFirstLane())
                    InterlockedOr(buf_LightTiles[tileIndex].aoSphereIdMasks[bucketIdx], setBits);
            }
        }
    }

    // Stage 4: Cull AO boxes against the frustum
    if (groupIndex < LightBufferLoader.GetAOBoxCount() && groupIndex < 64) {
        uint boxIdx = groupIndex;
        AOBox box = LightBufferLoader.GetAOBox(boxIdx);

        float3 scale = box.Scale + float3(0.5);
        float4x4 transform = box.InverseTransform;

        bool inFrustum = frustumContainsOBB(scale, transform);

        uint bucketIdx = boxIdx / 32;
        uint bucketBit = boxIdx % 32;
        uint maxBucketId = WaveActiveMax(bucketIdx);

        // Subgroup fun again
        for (int i = 0; i <= maxBucketId; i++) {
            if (i == bucketIdx) {
                uint setBits = WaveReadLaneFirst(WaveActiveBitOr(uint(inFrustum) << bucketBit));
                if (WaveIsFirstLane())
                    InterlockedOr(buf_LightTiles[tileIndex].aoBoxIdMasks[bucketIdx], setBits);
            }
        }
    }
}

void cullCubemaps(uint tileIndex, uint groupIndex) {
    if (groupIndex < LightBufferLoader.GetCubemapCount() && groupIndex > 0) {
        uint cubemapIdx = groupIndex;
        Cubemap c = LightBufferLoader.GetCubemap(cubemapIdx);

        float3 mi = c.Position - c.Extent;
        float3 ma = c.Position + c.Extent;
        bool inFrustum = frustumContainsAABB(mi, ma);

        uint bucketIdx = cubemapIdx / 32;
        uint bucketBit = cubemapIdx % 32;
        uint maxBucketId = WaveActiveMax(bucketIdx);

        // Subgroup fun again
        for (int i = 0; i <= maxBucketId; i++) {
            if (i == bucketIdx) {
                //uint setBits = subgroupBroadcastFirst(subgroupOr(uint(inFrustum) << bucketBit));
                //if (subgroupElect())
                    InterlockedOr(buf_LightTiles[tileIndex].cubemapIdMasks[bucketIdx], uint(inFrustum) << bucketBit);
            }
        }
    }
}

float depthBufferLoad(int2 coord) {
#ifdef MSAA
    return depthBuffer.Load(int3(coord, eyeIdx), 0).x;
#else
    return depthBuffer.Load(int4(coord, eyeIdx, 0)).x;
#endif
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 groupID : SV_GroupID, uint groupIndex : SV_GroupIndex, uint3 dispatchID : SV_DispatchThreadID) {
    uint x = groupID.x;
    uint y = groupID.y;
    uint tileIndex = ((y * buf_LightTileInfo.numTilesX) + x) + (eyeIdx * buf_LightTileInfo.tilesPerEye);

    clearBuffers(tileIndex, groupIndex);

    // Stage 1: Determine the depth bounds of the tile using atomics.
    // THIS ONLY WORKS FOR 16x16 TILES.
    // Changing the tile size means that there's no longer a 1:1 correlation between threads
    // and tile pixels, so this atomic depth read won't work.
    //
    // NOTE: When MSAA is on, the last parameter refers to the MSAA sample to load. When MSAA is
    // off, it refers to the mipmap level to load from.
//#define TILES_16
#ifdef TILES_16
    float depthAtCurrent = texelFetch(depthBuffer, int3(dispatchID.xy, eyeIdx), 0).x;
#else
    float depthAtCurrent = 1.0;
    int2 coord = int2(dispatchID.xy * 2);
    depthAtCurrent = min(depthAtCurrent, depthBufferLoad(coord));
    depthAtCurrent = min(depthAtCurrent, depthBufferLoad(coord + int2(0, 1)));
    depthAtCurrent = min(depthAtCurrent, depthBufferLoad(coord + int2(1, 1)));
    depthAtCurrent = min(depthAtCurrent, depthBufferLoad(coord + int2(1, 0)));
#endif
    uint depthAsUint = asuint(depthAtCurrent);

    // A depth of 0 only occurs when the skybox is visible.
    // Since the skybox can't receive lighting, there's no point in increasing
    // the depth bounds of the tile to receive the lighting.
    if (depthAtCurrent > 0.0f) {
        uint sgMinDepth = WaveActiveMin(depthAsUint);
        uint sgMaxDepth = WaveActiveMax(depthAsUint);
        if (WaveIsFirstLane()) {
            InterlockedMin(minDepthU, sgMinDepth);
            InterlockedMax(maxDepthU, sgMaxDepth);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Stage 2: Calculate frustum points.
    if (groupIndex == 0) {
        setupTile(tileIndex, x, y);
    }

    GroupMemoryBarrierWithGroupSync();

    cullAO(tileIndex, groupIndex);
    cullCubemaps(tileIndex, groupIndex);
    cullLights(tileIndex, groupIndex);
}
