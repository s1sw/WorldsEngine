#ifndef PARALLAX_HEADER
#define PARALLAX_HEADER

vec2 parallaxMapping(vec2 texCoords, vec3 viewDir, sampler2D depthMap, float heightScale, float minLayers, float maxLayers) {
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));

    float layerDepth = 1.0 / numLayers;
    float currentDepth = 0.0;
    vec2 P = viewDir.xy / viewDir.z * heightScale;
    P.y = -P.y;
    vec2 deltaTexCoords = P / numLayers;

    vec2  currentTexCoords     = texCoords;
    float currentDepthMapValue = 1.0 - pow(texture(depthMap, currentTexCoords).r, 1.0 / 2.2);

    while(currentDepth < currentDepthMapValue) {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = 1.0 - pow(texture(depthMap, currentTexCoords).r, 1.0 / 2.2);
        currentDepth += layerDepth;
    }

    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    float afterDepth  = currentDepthMapValue - currentDepth;
    float beforeDepth = (1.0 - texture(depthMap, prevTexCoords).r) - currentDepth + layerDepth;

    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

#endif
