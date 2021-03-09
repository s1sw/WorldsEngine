#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 inUV;

layout(push_constant) uniform PushConstants {
	vec4 texScaleOffset;
    // (x: model matrix index, y: material index, z: vp index, w: object id)
	ivec4 ubIndices;
    ivec2 pixelPickCoords;
};

struct Material {
	// (metallic, roughness, albedo texture index, unused)
	vec4 pack0;
	// (albedo color rgb, unused)
	vec4 pack1;
};

layout(std140, binding = 2) uniform MaterialSettingsBuffer {
    Material materials[256];
};

layout (binding = 4) uniform sampler2D albedoSampler[];

void main() {
    Material mat = materials[ubIndices.y];
    FragColor = vec4(1.0 - texture(albedoSampler[int(mat.pack0.z)], (inUV * texScaleOffset.xy) + texScaleOffset.zw).xyz, 1.0);
}