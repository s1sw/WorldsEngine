#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec3 inTexCoords;

layout (binding = 1) uniform samplerCube cubemap;

layout (push_constant) uniform PushConstants {
    // (x: vp index, y: cubemap index)
    ivec2 ubIndices;
    float boost;
};

void main() {
    FragColor = vec4(textureLod(cubemap, inTexCoords, 0).xyz * boost, 1.0);
}
