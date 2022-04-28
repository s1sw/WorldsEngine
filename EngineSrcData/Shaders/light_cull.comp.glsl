#version 450
#include <light.glsl>
#include <aobox.glsl>
#include <aosphere.glsl>
#include <cubemap.glsl>

#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_ARB_shader_ballot : require
layout (local_size_x = 16, local_size_y = 16) in;


layout (binding = 0) uniform LightTileInfo {
    uint tileSize;
    uint tilesPerEye;
    uint numTilesX;
    uint numTilesY;
} buf_LightTileInfo;

layout(std430, binding = 1) readonly buffer LightBuffer {
    mat4 otherShadowMatrices[4];

    uint lightCount;
    uint aoBoxCount;
    uint aoSphereCount;
    uint cubemapCount;

    vec4 cascadeTexelsPerUnit;
    mat4 dirShadowMatrices[4];

    Light lights[256];
    AOBox aoBox[128];
    AOSphere aoSphere[16];
    uint sphereIds[16];
    Cubemap cubemaps[64];
} buf_Lights;

layout (binding = 2) uniform MultiVP {
    mat4 view[2];
    mat4 projection[2];
    vec4 viewPos[2];
};

#ifdef TILE_DEPTH_SHADER
#ifdef MSAA
layout (binding = 3) uniform sampler2DMSArray depthBuffer;
#else
layout (binding = 3) uniform sampler2DArray depthBuffer;
#endif
#endif

layout (binding = 4) buffer TileLightCounts {
    uint tileLightCounts[];
} buf_LightTileLightCounts;

layout (binding = 5) buffer TileLightTiles {
    LightingTile tiles[];
} buf_LightTiles;

layout(push_constant) uniform PC {
    mat4 invViewProj;
    uint screenWidth;
    uint screenHeight;
    uint eyeIdx;
};

struct Frustum {
    vec4 planes[6];
};

struct AABB {
    vec3 center;
    vec3 extents;
};

#define CULL_DEPTH
#define CULL_AABB
#define USE_SUBGROUPS

bool frustumContainsSphere(in LightingTile tile, vec3 spherePos, float sphereRadius) {
    bool inside = true;
    for (int i = 0; i < 6; i++) {
        float dist = dot(spherePos, tile.frustumPlanes[i].xyz) + tile.frustumPlanes[i].w;

        if (dist < -sphereRadius)
            inside = false;
    }

    return inside;
}

bool aabbContainsSphere(in LightingTile tile, vec3 spherePos, float sphereRadius) {
    vec3 vDelta = max(vec3(0.0), abs(tile.aabbCenter - spherePos) - tile.aabbExtents);

    float fDistSq = dot(vDelta, vDelta);

    return fDistSq <= sphereRadius * sphereRadius;
}

bool frustumContainsOBB(in LightingTile tile, vec3 boxSize, mat4 transform) {
    // We can determine if the frustum contains an OBB by checking if it contains
    // any vertices of the OBB.

    for (int i = 0; i < 6; i++) {
        int outside = 0;

        for (int j = 0; j < 8; j++) {
            vec3 v = vec3(j % 2 == 0 ? -1.0 : 1.0, (j >> 1) % 2 == 0 ? -1.0 : 1.0, j < 4 ? -1.0 : 1.0);
            v = (transform * vec4(v * boxSize, 1.0)).xyz;
            outside += (dot(tile.frustumPlanes[i], vec4(v, 1.0)) < 0.0) ? 1 : 0;
        }

        if (outside == 8) return false;
    }

    return true;
}

vec3 aabbPoint(int index, vec3 min, vec3 max) {
    return vec3(index % 2 == 0 ? min.x : max.x, (index >> 1) % 2 == 0 ? min.y : max.y, index < 4 ? min.z : max.z);
}

bool frustumContainsAABB(in LightingTile tile, vec3 min, vec3 max) {
    vec3 points[8] = vec3[](
        min,
        vec3(max.x, min.y, min.z),
        vec3(min.x, max.y, min.z),
        vec3(max.x, max.y, min.z),
        vec3(min.x, min.y, max.z),
        vec3(max.x, min.y, max.z),
        vec3(min.x, max.y, max.z),
        vec3(max.x, max.y, max.z)
    );

    for (int i = 0; i < 6; i++) {
        bool inside = false;

        for (int j = 0; j < 8; j++) {
            if (dot(tile.frustumPlanes[i], vec4(points[j], 1.0)) > 0.0) {
                inside = true;
                break;
            }
        }

        if (!inside) return false;
    }

    //int outside[6] = int[](0, 0, 0, 0, 0, 0);
    //for (int i = 0; i < 8; i++) {
    //    // on each axis...
    //    for (int j = 0; j < 3; j++) {
    //        outside[j] += int(points[i][j] > max[j]);
    //        outside[j + 3] += int(points[i][j] < min[j]);
    //    }
    //}

    //for (int i = 0; i < 6; i++) {
    //    if (outside[i] == 8) return false;
    //}

    return true;
}

vec3 getTileMin(in LightingTile tile) {
    return min(tile.aabbCenter + tile.aabbExtents, tile.aabbCenter - tile.aabbExtents);
}

vec3 getTileMax(in LightingTile tile) {
    return max(tile.aabbCenter + tile.aabbExtents, tile.aabbCenter - tile.aabbExtents);
}

bool aabbContainsAABB(in LightingTile tile, vec3 min, vec3 max) {
    vec3 tileMin = getTileMin(tile);
    vec3 tileMax = getTileMax(tile);

    for (int i = 0; i < 3; i++) {
        if (!(tileMin[i] <= max[i] && tileMax[i] >= min[i])) {
            return false;
        }
    }

    return true;
}


bool aabbContainsPoint(in LightingTile tile, vec3 point) {
    vec3 mi = getTileMin(tile);
    vec3 ma = getTileMax(tile);

    return
        point.x >= mi.x && point.x <= ma.x &&
        point.y >= mi.y && point.y <= ma.y &&
        point.z >= mi.z && point.z <= ma.z;
}

// Calculates frustum planes from the set of frustum points
void setupTileFrustum(inout LightingTile tile, vec3 frustumPoints[8]) {
    vec3 camPos = viewPos[eyeIdx].xyz;

    for (int i = 0; i < 4; i++) {
        vec3 planeNormal = cross(frustumPoints[i] - camPos, frustumPoints[i + 1] - camPos);
        planeNormal = normalize(planeNormal);
        tile.frustumPlanes[i] = vec4(planeNormal, -dot(planeNormal, frustumPoints[i]));
    }

    // Near plane
    {
        vec3 planeNormal = cross(frustumPoints[1] - frustumPoints[0], frustumPoints[3] - frustumPoints[0]);
        planeNormal = normalize(planeNormal);
        tile.frustumPlanes[4] = vec4(planeNormal, -dot(planeNormal, frustumPoints[0]));
    }

    // Far plane
    {
        vec3 planeNormal = cross(frustumPoints[7] - frustumPoints[4], frustumPoints[5] - frustumPoints[4]);
        planeNormal = normalize(planeNormal);
        tile.frustumPlanes[5] = vec4(planeNormal, -dot(planeNormal, frustumPoints[4]));
    }
}

// Calculates the tile's AABB from the set of frustum points
void setupTileAABB(inout LightingTile tile, vec3 frustumPoints[8]) {
    // Calculate tile AABB
    vec3 aabbMax = vec3(0.0f);
    vec3 aabbMin = vec3(10000000.0f);

    for (int i = 0; i < 8; i++) {
        aabbMax = max(aabbMax, frustumPoints[i]);
        aabbMin = min(aabbMin, frustumPoints[i]);
    }

    tile.aabbCenter = (aabbMax + aabbMin) * 0.5;
    tile.aabbExtents = (aabbMax - aabbMin) * 0.5;
}

void setupTile(uint tileIndex, uint x, uint y) {
    LightingTile tile = buf_LightTiles.tiles[tileIndex];
    vec3 camPos = viewPos[eyeIdx].xyz;
    float minDepth = uintBitsToFloat(tile.minDepthU);
    float maxDepth = uintBitsToFloat(tile.maxDepthU);

    float tileSize = buf_LightTileInfo.tileSize;
    vec2 ndcTileSize = 2.0f * vec2(tileSize, -tileSize) / vec2(screenWidth, screenHeight);

    // Calculate frustum
    vec2 ndcTopLeftCorner = vec2(-1.0f, 1.0f);
    vec2 tileCoords = vec2(x, y);

    vec2 ndcTileCorners[4] = {
        ndcTopLeftCorner + ndcTileSize * tileCoords, // Top left
        ndcTopLeftCorner + ndcTileSize * (tileCoords + vec2(1, 0)), // Top right
        ndcTopLeftCorner + ndcTileSize * (tileCoords + vec2(1, 1)), // Bottom right
        ndcTopLeftCorner + ndcTileSize * (tileCoords + vec2(0, 1)), // Bottom left
    };

    mat4 invVP = inverse(projection[eyeIdx] * view[eyeIdx]);
    vec3 frustumPoints[8];

#ifdef CULL_DEPTH
    float nearZ = maxDepth;
    float farZ = minDepth;
#else
    float nearZ = 1.0f;
    float farZ = 0.000001f;
#endif

    // Find the points on the near plane
    for (int i = 0; i < 4; i++) {
        vec4 projected = invVP * vec4(ndcTileCorners[i], nearZ, 1.0f);
        frustumPoints[i] = vec3(projected) / projected.w;
    }

    for (int i = 0; i < 4; i++) {
        vec4 projected = invVP * vec4(ndcTileCorners[i], farZ, 1.0f);
        frustumPoints[i + 4] = vec3(projected) / projected.w;
    }

    setupTileFrustum(tile, frustumPoints);
    setupTileAABB(tile, frustumPoints);
    buf_LightTiles.tiles[tileIndex] = tile;
}

void cullLights(uint tileIndex) {
    // Stage 3: Cull lights against the frustum. At this point,
    // one invocation = one light. Since the light array is always
    // tightly packed, we can just check if the index is less than
    // the light count.
    LightingTile tile = buf_LightTiles.tiles[tileIndex];
    if (gl_LocalInvocationIndex < buf_Lights.lightCount) {
        uint lightIndex = gl_LocalInvocationIndex;
        Light light = buf_Lights.lights[lightIndex];
        vec3 lightPosition = light.pack2.xyz;

        uint lightType = getLightType(light);

        if (lightType == LT_TUBE) {
            // Take the average position of the two end points
            lightPosition = (light.pack1.xyz + light.pack2.xyz) * 0.5;
        }

        float lightRadius = buf_Lights.lights[lightIndex].distanceCutoff;

#ifdef CULL_AABB
        bool inFrustum = frustumContainsSphere(tile, lightPosition, lightRadius) && aabbContainsSphere(tile, lightPosition, lightRadius);
#else
        bool inFrustum = frustumContainsSphere(tile, lightPosition, lightRadius);
#endif

        uint bucketIdx = lightIndex / 32;
        uint bucketBit = lightIndex % 32;

        uint maxBucketId = subgroupMax(bucketIdx);

        // Subgroup fun: take the maximum bucket ID, perform an or operation
        // along the subgroup's light bitmask and then elect a single invocation
        // to perform the atomic.
        // This results in a *tiny* performance uplift.
#ifdef USE_SUBGROUPS
        for (int i = 0; i <= maxBucketId; i++) {
            if (i == bucketIdx) {
                uint setBits = subgroupBroadcastFirst(subgroupOr(uint(inFrustum || lightType == LT_DIRECTIONAL) << bucketBit));
                if (subgroupElect())
                    atomicOr(buf_LightTiles.tiles[tileIndex].lightIdMasks[i], setBits);
            }
        }
#else
        uint setBits = uint(inFrustum || lightType == LT_DIRECTIONAL) << bucketBit;
        atomicOr(buf_LightTiles.tiles[tileIndex].lightIdMasks[bucketIdx], setBits);
#endif

#ifdef DEBUG
        //if (inFrustum)
            //atomicAdd(buf_LightTileLightCounts.tileLightCounts[tileIndex], 1);
#endif
    }
}

void cullAO(uint tileIndex) {
    // Stage 3: Cull AO spheres against the frustum
    LightingTile tile = buf_LightTiles.tiles[tileIndex];
    if (gl_LocalInvocationIndex < buf_Lights.aoSphereCount) {
        uint sphereIndex = gl_LocalInvocationIndex;
        AOSphere sph = buf_Lights.aoSphere[sphereIndex];
        vec3 spherePos = sph.position;
        float cullRadius = sph.radius + 1.0f;

#ifdef CULL_AABB
        bool inFrustum = frustumContainsSphere(tile, spherePos, cullRadius) && aabbContainsSphere(tile, spherePos, cullRadius);
#else
        bool inFrustum = frustumContainsSphere(tile, spherePos, cullRadius);
#endif

        uint bucketIdx = sphereIndex / 32;
        uint bucketBit = sphereIndex % 32;
        uint maxBucketId = subgroupMax(bucketIdx);

        // Subgroup fun again
        for (int i = 0; i <= maxBucketId; i++) {
            if (i == bucketIdx) {
                uint setBits = subgroupBroadcastFirst(subgroupOr(uint(inFrustum) << bucketBit));
                if (subgroupElect())
                    atomicOr(buf_LightTiles.tiles[tileIndex].aoSphereIdMasks[bucketIdx], setBits);
            }
        }
    }

    // Stage 4: Cull AO boxes against the frustum
    if (gl_LocalInvocationIndex < buf_Lights.aoBoxCount && gl_LocalInvocationIndex < 64) {
        uint boxIdx = gl_LocalInvocationIndex;
        AOBox box = buf_Lights.aoBox[boxIdx];

        vec3 scale = getBoxScale(box) + vec3(0.5);
        mat4 transform = getBoxInverseTransform(box);

        bool inFrustum = frustumContainsOBB(tile, scale, transform);

        uint bucketIdx = boxIdx / 32;
        uint bucketBit = boxIdx % 32;
        uint maxBucketId = subgroupMax(bucketIdx);

        // Subgroup fun again
        for (int i = 0; i <= maxBucketId; i++) {
            if (i == bucketIdx) {
                uint setBits = subgroupBroadcastFirst(subgroupOr(uint(inFrustum) << bucketBit));
                if (subgroupElect())
                    atomicOr(buf_LightTiles.tiles[tileIndex].aoBoxIdMasks[bucketIdx], setBits);
            }
        }
    }
}

void cullCubemaps(uint tileIndex) {
    LightingTile tile = buf_LightTiles.tiles[tileIndex];
    if (gl_LocalInvocationIndex < buf_Lights.cubemapCount && gl_LocalInvocationIndex > 0) {
        uint cubemapIdx = gl_LocalInvocationIndex;
        Cubemap c = buf_Lights.cubemaps[cubemapIdx];

        vec3 mi = c.position - c.extent;
        vec3 ma = c.position + c.extent;
        bool inFrustum = frustumContainsAABB(tile, mi, ma);

        uint bucketIdx = cubemapIdx / 32;
        uint bucketBit = cubemapIdx % 32;
        uint maxBucketId = subgroupMax(bucketIdx);

        // Subgroup fun again
        for (int i = 0; i <= maxBucketId; i++) {
            if (i == bucketIdx) {
                uint setBits = subgroupOr(uint(inFrustum) << bucketBit);
                if (subgroupElect())
                    atomicOr(buf_LightTiles.tiles[tileIndex].cubemapIdMasks[bucketIdx], setBits);
            }
        }
    }
}

void clearTile(uint tileIndex) {
    LightingTile tile = buf_LightTiles.tiles[tileIndex];
    for (int i = 0; i < 8; i++) {
        tile.lightIdMasks[i] = 0u;
    }
    for (int i = 0; i < 2; i++) {
        tile.aoSphereIdMasks[i] = 0u;
        tile.aoBoxIdMasks[i] = 0u;
        tile.cubemapIdMasks[i] = 0u;
    }

    tile.minDepthU = floatBitsToUint(1.0);
    tile.maxDepthU = 0u;
    buf_LightTiles.tiles[tileIndex] = tile;
    buf_LightTileLightCounts.tileLightCounts[tileIndex] = 0;

    // We don't need to clear the other values in the tile, because they're
    // always written to.
}

#ifdef TILE_DEPTH_SHADER
float minIfInBounds(float depthAtCurrent, ivec2 bounds, ivec2 coord) {
    if (coord.x < bounds.x && coord.y < bounds.y) {
        return min(depthAtCurrent, texelFetch(depthBuffer, ivec3(coord, eyeIdx), 0).x);
    }
    return depthAtCurrent;
}
void calcTileDepth(uint tileIndex) {
    // Stage 1: Determine the depth bounds of the tile using atomics.
    // THIS ONLY WORKS FOR 16x16 TILES.
    // Changing the tile size means that there's no longer a 1:1 correlation between threads
    // and tile pixels, so this atomic depth read won't work.
    //
    // NOTE: When MSAA is on, the last parameter refers to the MSAA sample to load. When MSAA is
    // off, it refers to the mipmap level to load from.
#ifdef MSAA
    ivec2 bounds = textureSize(depthBuffer).xy;
#else
    ivec2 bounds = textureSize(depthBuffer, 0).xy;
#endif

//#define TILES_16
#ifdef TILES_16
    float depthAtCurrent = texelFetch(depthBuffer, ivec3(gl_GlobalInvocationID.xy, eyeIdx), 0).x;
#else
    float depthAtCurrent = 1.0;
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy * 2);
    //depthAtCurrent = min(depthAtCurrent, texelFetch(depthBuffer, ivec3(coord, eyeIdx), 0).x);
    //depthAtCurrent = min(depthAtCurrent, texelFetch(depthBuffer, ivec3(coord + ivec2(0, 1), eyeIdx), 0).x);
    //depthAtCurrent = min(depthAtCurrent, texelFetch(depthBuffer, ivec3(coord + ivec2(1, 1), eyeIdx), 0).x);
    //depthAtCurrent = min(depthAtCurrent, texelFetch(depthBuffer, ivec3(coord + ivec2(1, 0), eyeIdx), 0).x);

    if (coord.x >= bounds.x || coord.y >= bounds.y) {
        return;
    }

    depthAtCurrent = minIfInBounds(depthAtCurrent, bounds, coord);
    depthAtCurrent = minIfInBounds(depthAtCurrent, bounds, coord + ivec2(0, 1));
    depthAtCurrent = minIfInBounds(depthAtCurrent, bounds, coord + ivec2(1, 1));
    depthAtCurrent = minIfInBounds(depthAtCurrent, bounds, coord + ivec2(1, 0));
#endif
    uint depthAsUint = floatBitsToUint(depthAtCurrent);

    // A depth of 0 only occurs when the skybox is visible.
    // Since the skybox can't receive lighting, there's no point in increasing
    // the depth bounds of the tile to receive the lighting.
    if (depthAtCurrent > 0.0f) {
        //uint sgMinDepth = subgroupMin(depthAsUint);
        //uint sgMaxDepth = subgroupMax(depthAsUint);

        //if (subgroupElect()) {
        //    atomicMin(buf_LightTiles.tiles[tileIndex].minDepthU, sgMinDepth);
        //    atomicMax(buf_LightTiles.tiles[tileIndex].maxDepthU, sgMaxDepth);
        //}
        atomicMin(buf_LightTiles.tiles[tileIndex].minDepthU, depthAsUint);
        atomicMax(buf_LightTiles.tiles[tileIndex].maxDepthU, depthAsUint);
        buf_LightTileLightCounts.tileLightCounts[tileIndex] = uint(uintBitsToFloat(buf_LightTiles.tiles[tileIndex].maxDepthU) * 200.0);

    }
}
#endif

void main() {
#if defined(CLEAR_SHADER)
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    uint tileIndex = ((y * buf_LightTileInfo.numTilesX) + x) + (eyeIdx * buf_LightTileInfo.tilesPerEye);

    if (x >= buf_LightTileInfo.numTilesX || y >= buf_LightTileInfo.numTilesY) return;

    clearTile(tileIndex);
#elif defined(TILE_DEPTH_SHADER)
    uint x = gl_WorkGroupID.x;
    uint y = gl_WorkGroupID.y;
    uint tileIndex = ((y * buf_LightTileInfo.numTilesX) + x) + (eyeIdx * buf_LightTileInfo.tilesPerEye);

    if (x >= buf_LightTileInfo.numTilesX || y >= buf_LightTileInfo.numTilesY) return;

    calcTileDepth(tileIndex);
#elif defined(TILE_SETUP_SHADER)
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    uint tileIndex = ((y * buf_LightTileInfo.numTilesX) + x) + (eyeIdx * buf_LightTileInfo.tilesPerEye);

    if (x >= buf_LightTileInfo.numTilesX || y >= buf_LightTileInfo.numTilesY) return;

    setupTile(tileIndex, x, y);
#elif defined(TILE_CULL_SHADER)
    uint x = gl_WorkGroupID.x;
    uint y = gl_WorkGroupID.y;
    uint tileIndex = ((y * buf_LightTileInfo.numTilesX) + x) + (eyeIdx * buf_LightTileInfo.tilesPerEye);

    if (x >= buf_LightTileInfo.numTilesX || y >= buf_LightTileInfo.numTilesY) return;

    cullLights(tileIndex);
    cullAO(tileIndex);
    cullCubemaps(tileIndex);
#else
#error What???
#endif
}
