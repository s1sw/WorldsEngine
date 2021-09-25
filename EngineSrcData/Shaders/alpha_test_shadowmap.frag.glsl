#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#include <material.glsl>

layout (location = 0) in vec2 inUV;

layout (binding = 0) uniform sampler2D tex2dSampler[];
layout(std140, binding = 1) readonly buffer MaterialSettingsBuffer {
    Material materials[256];
};

layout(push_constant) uniform PushConstants {
    mat4 mvp;
	uint matIdx;
};

float mipMapLevel() {
#if 0
    return textureQueryLod(tex2dSampler[materials[matIdx].albedoTexIdx], inUV).x;
#else
    vec2 dx = dFdx(inUV);
    vec2 dy = dFdy(inUV);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));
    
    return max(0.0, 0.5 * log2(delta_max_sqr));
#endif
}

void main() {
    float alpha = 1.0f;
    Material mat = materials[matIdx];
    float alphaCutoff = (mat.cutoffFlags & (0xFF)) / 255.0f;

    if (alphaCutoff > 0.0f) {
        alpha = texture(tex2dSampler[mat.albedoTexIdx], inUV).a;
        //alpha *= 1 + mipMapLevel() * 0.75;
        alpha = (alpha - alphaCutoff) / max(fwidth(alpha), 0.0001) + 0.5;
    }
	
    if (alpha < 0.5)
		discard;
}
