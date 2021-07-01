#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#include <standard_descriptors.glsl>
#include <standard_push_constants.glsl>

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 finalCol;

void main() {
    float alpha = 1.0f;
    Material mat = materials[matIdx];
    float alphaCutoff = (mat.cutoffFlags & (0xFF)) / 255.0f;

    if (alphaCutoff > 0.0f) {
        alpha = texture(tex2dSampler[mat.albedoTexIdx], inUV).a;
        alpha = (alpha - alphaCutoff) / max(fwidth(alpha), 0.0001) + 0.5;
        if (alpha < 0.5)
            discard;
    }

    finalCol = vec4(1.0, 1.0, 1.0, alpha);
}
