#version 450
#include <math.glsl>

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 FragColor;

layout(binding = 1) uniform sampler2D texSampler;

float aastep(float threshold, float value) {
    float afwidth = length(vec2(dFdx(value), dFdy(value))) * 0.70710678118654757;
    return smoothstep(threshold-afwidth, threshold+afwidth, value);
}

void main() {
    vec2 tCoord = vec2(inUv.x, inUv.y);
    float distance = textureLod(texSampler, tCoord, 0.0).r;

    // sdf distance from edge (scalar)
    float dist = (0.2 - distance);

    // sdf distance per pixel (gradient vector)
    vec2 ddist = vec2(dFdx(dist), dFdy(dist));

    // distance to edge in pixels (scalar)
    float pixelDist = dist / length(ddist);

    FragColor = vec4(vec3(1.0), saturate(0.5 - pixelDist));
}
