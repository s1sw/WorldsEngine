#version 450
#include <light.glsl>
#include <aobox.glsl>
#include <aosphere.glsl>

layout (local_size_x = 128, local_size_y = 1) in;

struct LightingTile {
    uint lightIds[128];
};

layout (binding = 0) buffer LightTileBuffer {
    uint tileSize;
    uint tilesPerEye;
    uint numTilesX;
    uint numTilesY;
    uint tileLightCounts[16384];
    LightingTile tiles[16384];
} buf_LightTiles;

layout (std430, binding = 1) readonly buffer LightBuffer {
    mat4 otherShadowMatrices[4];
    // (light count, yzw cascade texels per unit)
    vec4 pack0;
    // (ao box count, ao sphere count, zw unused)
    vec4 pack1;
    mat4 dirShadowMatrices[3];
    Light lights[128];
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

layout(push_constant) uniform PC {
    uint screenWidth;
    uint screenHeight;
    uint eyeIdx;
};

struct Frustum {
    vec4 planes[6];
};

shared Frustum tileFrustum;

bool containsSphere(vec3 spherePos, float sphereRadius) {
    for (int i = 0; i < 6; i++) {
        float dist = dot(spherePos, tileFrustum.planes[i].xyz) + tileFrustum.planes[i].w;

        if (dist < -sphereRadius)
            return false;
    }

    return true;
}

void main() {
    uint x = gl_WorkGroupID.x;
    uint y = gl_WorkGroupID.y;
    uint tileIndex = (y * buf_LightTiles.numTilesX) + x;

    buf_LightTiles.tiles[tileIndex].lightIds[gl_LocalInvocationIndex] = ~0u;

    if (gl_LocalInvocationIndex >= buf_Lights.pack0.x) {
        return;
    }

    // Stage 1: Calculate the frustum for this workgroup
    if (gl_LocalInvocationIndex == 0) {
        buf_LightTiles.tileLightCounts[tileIndex] = 0;
        float tileSize = buf_LightTiles.tileSize;
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
            // Projection matrices are reverse-Z so use 1.0 for Z
            vec4 projected = invProjView * vec4(ndcTileCorners[i], 1.0f, 1.0f);
            frustumPoints[i] = vec3(projected) / projected.w;

            // And also on the far plane
            // Use a really small value for Z, otherwise we'll get infinity
            projected = invProjView * vec4(ndcTileCorners[i], 0.0000001f, 1.0f);
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
    }
    barrier();

    // Stage 2: Cull lights against the frustum
    uint lightIndex = gl_LocalInvocationIndex;
    Light light = buf_Lights.lights[lightIndex];
    vec3 lightPosition = light.pack2.xyz;

    int lightType = int(light.pack0.w);

    if (lightType == LT_TUBE) {
        // Take the average position of the two end points
        lightPosition = (light.pack1.xyz + light.pack2.xyz) * 0.5;
    }

    float lightRadius = buf_Lights.lights[lightIndex].distanceCutoff;

    if (containsSphere(lightPosition, lightRadius)) {
        buf_LightTiles.tiles[tileIndex].lightIds[gl_LocalInvocationIndex] = lightIndex;
    }

    memoryBarrier();
    // Stage 3: Compact
    if (gl_LocalInvocationIndex == 0) {
        int currentInsertionPoint = 0;
        for (int i = 0; i < buf_Lights.pack0.x; i++) {
            uint thisLightId = buf_LightTiles.tiles[tileIndex].lightIds[i];
            if (thisLightId == ~0u) continue;

            buf_LightTiles.tiles[tileIndex].lightIds[currentInsertionPoint] = thisLightId;
            currentInsertionPoint++;
        }

        buf_LightTiles.tileLightCounts[tileIndex] = currentInsertionPoint;
    }
}
