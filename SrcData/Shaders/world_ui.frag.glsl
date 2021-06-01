#version 450

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 FragColor;

layout(binding = 1) uniform sampler2D texSampler;

float aastep(float threshold, float value) {
    float afwidth = length(vec2(dFdx(value), dFdy(value))) * 0.70710678118654757;
    return smoothstep(threshold-afwidth, threshold+afwidth, value);
}

void main() {
    vec2 tCoord = vec2(inUv.x, inUv.y);
    float distance = texture(texSampler, tCoord).r;

    float alpha = aastep(0.225, distance);
    FragColor = vec4(vec3(1.0), alpha);
    //FragColor = vec4(vec3(distance), 1.0);
}
