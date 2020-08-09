#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;

layout (location = 0) out vec3 outTexCoords;

layout (binding = 0) uniform MultiVP {
    mat4 view[8];
	mat4 projection[8];
    vec4 viewPos[8];
};

layout (push_constant) uniform PushConstants {
    // (x: vp index, y: cubemap index)
    ivec4 ubIndices; 
};

void main() {
    gl_Position = projection[ubIndices.y + gl_ViewIndex] * view[ubIndices.y + gl_ViewIndex] * vec4(inPos, 1.0); // Apply MVP transform
    outTexCoords = inPos;
}
