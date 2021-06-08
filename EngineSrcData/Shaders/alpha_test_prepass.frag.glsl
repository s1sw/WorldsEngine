#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 finalCol;

struct Material {
	// (metallic, roughness, albedo texture index, normal texture index)
	//vec4 pack0;
    float metallic;
    float roughness;
    int albedoTexIdx;
    int normalTexIdx;
	// (albedo color rgb, alpha cutoff)
    vec3 albedoColor;
    float alphaCutoff;
    int heightmapIdx;
    float heightScale;
};

layout(std140, binding = 2) uniform MaterialSettingsBuffer {
    Material materials[256];
};

layout (binding = 4) uniform sampler2D tex2dSampler[];

layout(push_constant) uniform PushConstants {
	vec4 texScaleOffset;
    int modelMatrixIdx;
    int matIdx;
    int vpIdx;
    uint objectId;
    ivec2 pixelPickCoords;
    uint doPicking;
};

void main() {
    float alpha = 1.0f;
    Material mat = materials[matIdx];
    if (mat.alphaCutoff > 0.0f) {
        alpha = texture(tex2dSampler[mat.albedoTexIdx], inUV).a;

        alpha = (alpha - mat.alphaCutoff) / max(fwidth(alpha), 0.0001) + 0.5;

        
    }

    finalCol = vec4(1.0, 1.0, 1.0, alpha);
}