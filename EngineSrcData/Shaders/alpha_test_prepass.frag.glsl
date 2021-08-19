#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#include <standard_descriptors.glsl>
#include <standard_push_constants.glsl>

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 finalCol;

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
	
    finalCol = vec4(1.0, 1.0, 1.0, alpha);
}
