#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#include <material.glsl>

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 inUV;

layout(push_constant) uniform PushConstants {
	vec4 texScaleOffset;
    // (x: model matrix index, y: material index, z: vp index, w: object id)
	ivec4 ubIndices;
    ivec2 pixelPickCoords;
};

layout(std140, binding = 2) buffer MaterialSettingsBuffer {
    Material materials[256];
};

layout (binding = 4) uniform sampler2D albedoSampler[];

void main() {
    Material mat = materials[ubIndices.y];
    FragColor = vec4(1.0 - texture(albedoSampler[mat.albedoTexIdx], (inUV * texScaleOffset.xy) + texScaleOffset.zw).xyz, 1.0);
}
