#version 450
#include <light.glsl>
#include <aobox.glsl>
#include <aosphere.glsl>

#extension GL_KHR_shader_subgroup_arithmetic : require
layout (local_size_x = 16, local_size_y = 16) in;

struct LightingTile {
    uint lightIdMasks[8];
	uint aoBoxIdMasks[2];
	uint aoSphereIdMasks[2];
};

layout (binding = 0) uniform LightTileInfo {
    uint tileSize;
    uint tilesPerEye;
    uint numTilesX;
    uint numTilesY;
} buf_LightTileInfo;

layout (std430, binding = 1) readonly buffer LightBuffer {
    mat4 otherShadowMatrices[4];
    // (light count, yzw cascade texels per unit)
    vec4 pack0;
    // (ao box count, ao sphere count, zw unused)
    vec4 pack1;
    mat4 dirShadowMatrices[3];
    Light lights[256];
    AOBox aoBox[16];
    AOSphere aoSphere[16];
    uint sphereIds[16];
} buf_Lights;


layout (binding = 2) uniform MultiVP {
    mat4 view[2];
    mat4 projection[2];
    vec4 viewPos[2];
};

layout (binding = 3) uniform sampler2DMSArray depthBuffer;

layout (binding = 4) buffer TileLightCounts {
    uint tileLightCounts[];
} buf_LightTileLightCounts;

layout (binding = 5) buffer TileLightTiles {
    LightingTile tiles[];
} buf_LightTiles;

layout(push_constant) uniform PC {
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

shared Frustum tileFrustum;
shared AABB tileAABB;
shared uint minDepthU;
shared uint maxDepthU;

bool frustumContainsSphere(vec3 spherePos, float sphereRadius) {
    for (int i = 0; i < 6; i++) {
        float dist = dot(spherePos, tileFrustum.planes[i].xyz) + tileFrustum.planes[i].w;

        if (dist < -sphereRadius)
            return false;
    }

    return true;
}

bool aabbContainsSphere(vec3 spherePos, float sphereRadius) {
    vec3 vDelta = max(vec3(0.0), abs(tileAABB.center - spherePos) - tileAABB.extents);

    float fDistSq = dot(vDelta, vDelta);

    return fDistSq <= sphereRadius * sphereRadius;
}

bool frustumContainsOBB(vec3 boxSize, mat3 rotMat, vec3 pos) {
	// We can determine if the frustum contains an AABB by checking if it contains
	// any vertices of the AABB.
	
	vec3 v0 = ((vec3(-1.0,-1.0,-1.0) * boxSize)) + pos;
    vec3 v1 = ((vec3( 1.0,-1.0,-1.0) * boxSize)) + pos;
    vec3 v2 = ((vec3(-1.0, 1.0,-1.0) * boxSize)) + pos;
    vec3 v3 = ((vec3( 1.0, 1.0,-1.0) * boxSize)) + pos;
    vec3 v4 = ((vec3(-1.0,-1.0, 1.0) * boxSize)) + pos;
    vec3 v5 = ((vec3( 1.0,-1.0, 1.0) * boxSize)) + pos;
    vec3 v6 = ((vec3(-1.0, 1.0, 1.0) * boxSize)) + pos;
    vec3 v7 = ((vec3( 1.0, 1.0, 1.0) * boxSize)) + pos;
	
    for (int i = 0; i < 6; i++) {
		int outside = 0;
		
        outside += (dot(tileFrustum.planes[i], vec4(v0, 1.0)) < 0.0) ? 1 : 0;
        outside += (dot(tileFrustum.planes[i], vec4(v1, 1.0)) < 0.0) ? 1 : 0;
        outside += (dot(tileFrustum.planes[i], vec4(v2, 1.0)) < 0.0) ? 1 : 0;
        outside += (dot(tileFrustum.planes[i], vec4(v3, 1.0)) < 0.0) ? 1 : 0;
        outside += (dot(tileFrustum.planes[i], vec4(v4, 1.0)) < 0.0) ? 1 : 0;
        outside += (dot(tileFrustum.planes[i], vec4(v5, 1.0)) < 0.0) ? 1 : 0;
        outside += (dot(tileFrustum.planes[i], vec4(v6, 1.0)) < 0.0) ? 1 : 0;
        outside += (dot(tileFrustum.planes[i], vec4(v7, 1.0)) < 0.0) ? 1 : 0;
		
		if (outside == 8) return false;
    }

    return true;
}

void main() {
    if (gl_LocalInvocationIndex.x == 0) {
        minDepthU = floatBitsToUint(1.0);
        maxDepthU = 0u;
    }

    uint x = gl_WorkGroupID.x;
    uint y = gl_WorkGroupID.y;
    uint tileIndex = ((y * buf_LightTileInfo.numTilesX) + x) + (eyeIdx * buf_LightTileInfo.tilesPerEye);

    buf_LightTiles.tiles[tileIndex].lightIdMasks[gl_LocalInvocationIndex % 8] = 0u;
	buf_LightTiles.tiles[tileIndex].aoSphereIdMasks[gl_LocalInvocationIndex % 2] = 0u;
	buf_LightTiles.tiles[tileIndex].aoBoxIdMasks[gl_LocalInvocationIndex % 2] = 0u;
	
	barrier();

    // Stage 1: Determine the depth bounds of the tile using atomcs.
    // THIS ONLY WORKS FOR 16x16 TILES.
    // Changing the tile size means that there's no longer a 1:1 correlation between threads
    // and tile pixels, so this atomic depth read won't work.
    float depthAtCurrent = texelFetch(depthBuffer, ivec3(gl_GlobalInvocationID.xy, eyeIdx), 0).x;
    uint depthAsUint = floatBitsToUint(depthAtCurrent);

    // A depth of 0 only occurs when the skybox is visible.
    // Since the skybox can't receive lighting, there's no point in increasing
    // the depth bounds of the tile to receive the lighting.
    if (depthAsUint != 0) {
        atomicMin(minDepthU, depthAsUint);
        atomicMax(maxDepthU, depthAsUint);
    }

    barrier();

    // Stage 2: Calculate the frustum for this workgroup
    if (gl_LocalInvocationIndex == 0) {
        float minDepth = uintBitsToFloat(minDepthU);
        float maxDepth = uintBitsToFloat(maxDepthU);

        buf_LightTileLightCounts.tileLightCounts[tileIndex] = 0;
        float tileSize = buf_LightTileInfo.tileSize;
        vec2 ndcTileSize = 2.0f * vec2(tileSize, -tileSize) / vec2(screenWidth, screenHeight);
        vec3 camPos = viewPos[eyeIdx].xyz;

        mat4 invProjView = inverse(projection[eyeIdx] * view[eyeIdx]);

        // Calculate frustum
        vec2 ndcTopLeftCorner = vec2(-1.0f, 1.0f);
        vec2 tileCoords = vec2(x, y);

        vec2 ndcTileCorners[4] = {
            ndcTopLeftCorner + ndcTileSize * tileCoords, // Top left
            ndcTopLeftCorner + ndcTileSize * (tileCoords + vec2(1, 0)), // Top right
            ndcTopLeftCorner + ndcTileSize * (tileCoords + vec2(1, 1)), // Bottom right
            ndcTopLeftCorner + ndcTileSize * (tileCoords + vec2(0, 1)), // Bottom left
        };

        vec3 frustumPoints[8];

        for (int i = 0; i < 4; i++) {
            // Find the point on the near plane
#ifdef CULL_DEPTH
            float nearZ = maxDepth;
            float farZ = minDepth;
#else
            float nearZ = 1.0f;
            float farZ = 0.000001f;
#endif

            vec4 projected = invProjView * vec4(ndcTileCorners[i], nearZ, 1.0f);
            frustumPoints[i] = vec3(projected) / projected.w;

            // And also on the far plane
            projected = invProjView * vec4(ndcTileCorners[i], farZ, 1.0f);
            frustumPoints[i + 4] = vec3(projected / projected.w);
        }

        for (int i = 0; i < 4; i++) {
            vec3 planeNormal = cross(frustumPoints[i] - camPos, frustumPoints[i + 1] - camPos);
            planeNormal = normalize(planeNormal);
            tileFrustum.planes[i] = vec4(planeNormal, -dot(planeNormal, frustumPoints[i]));
        }

        // Near plane
        {
            vec3 planeNormal = cross(frustumPoints[1] - frustumPoints[0], frustumPoints[3] - frustumPoints[0]);
            planeNormal = normalize(planeNormal);
            tileFrustum.planes[4] = vec4(planeNormal, -dot(planeNormal, frustumPoints[0]));
        }

        // Far plane
        {
            vec3 planeNormal = cross(frustumPoints[7] - frustumPoints[4], frustumPoints[5] - frustumPoints[4]);
            planeNormal = normalize(planeNormal);
            tileFrustum.planes[5] = vec4(planeNormal, -dot(planeNormal, frustumPoints[4]));
        }

        // Calculate tile AABB
        vec3 aabbMax = vec3(0.0f);
        vec3 aabbMin = vec3(1000000000.0f);

        for (int i = 0; i < 8; i++) {
            aabbMax = max(aabbMax, frustumPoints[i]);
            aabbMin = min(aabbMin, frustumPoints[i]);
        }

        tileAABB.center = (aabbMax + aabbMin) * 0.5;
        tileAABB.extents = (aabbMax - aabbMin) * 0.5;
    }

    barrier();

    // Stage 2: Cull lights against the frustum
    if (gl_LocalInvocationIndex < buf_Lights.pack0.x) {
        uint lightIndex = gl_LocalInvocationIndex;
        Light light = buf_Lights.lights[lightIndex];
        vec3 lightPosition = light.pack2.xyz;

        int lightType = int(light.pack0.w);

        if (lightType == LT_TUBE) {
            // Take the average position of the two end points
            lightPosition = (light.pack1.xyz + light.pack2.xyz) * 0.5;
        }

        float lightRadius = buf_Lights.lights[lightIndex].distanceCutoff;

#ifdef CULL_AABB
        bool inFrustum = frustumContainsSphere(lightPosition, lightRadius) && aabbContainsSphere(lightPosition, lightRadius);
#else
        bool inFrustum = frustumContainsSphere(lightPosition, lightRadius);
#endif

        if (inFrustum || lightType == LT_DIRECTIONAL) {
			uint bucketIdx = lightIndex / 32;
			uint bucketBit = lightIndex % 32;
			atomicOr(buf_LightTiles.tiles[tileIndex].lightIdMasks[bucketIdx], 1 << bucketBit);
#ifdef DEBUG
			atomicAdd(buf_LightTileLightCounts.tileLightCounts[tileIndex], 1);
#endif
        }
    }
	
	// Stage 3: Cull AO spheres against the frustum
	if (gl_LocalInvocationIndex < buf_Lights.pack1.y) {
        uint sphereIndex = gl_LocalInvocationIndex;
        AOSphere sph = buf_Lights.aoSphere[sphereIndex];
        vec3 spherePos = sph.position;
		float cullRadius = sph.radius + 1.0f;

#ifdef CULL_AABB
        bool inFrustum = frustumContainsSphere(spherePos, cullRadius) && aabbContainsSphere(spherePos, cullRadius);
#else
        bool inFrustum = frustumContainsSphere(spherePos, cullRadius);
#endif

        if (inFrustum) {
			uint bucketIdx = sphereIndex / 32;
			uint bucketBit = sphereIndex % 32;
			atomicOr(buf_LightTiles.tiles[tileIndex].aoSphereIdMasks[bucketIdx], 1 << bucketBit);
        }
    }
	
	// Stage 3: Cull AO boxes against the frustum
	//if (gl_LocalInvocationIndex < buf_Lights.pack1.x) {
    //    uint boxIdx = gl_LocalInvocationIndex;
    //    AOBox box = buf_Lights.aoBox[boxIdx];
	//	
    //    bool inFrustum = frustumContainsOBB(getBoxScale(box), getBoxRotationMat(box), getBoxTranslation(box).zyx);
	//
    //    if (inFrustum) {
	//		uint bucketIdx = boxIdx / 32;
	//		uint bucketBit = boxIdx % 32;
	//		atomicOr(buf_LightTiles.tiles[tileIndex].aoBoxIdMasks[bucketIdx], 1 << bucketBit);
    //    }
    //}
}
