#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;

layout (binding = 0) uniform MultiVP {
    mat4 view[4];
    mat4 projection[4];
    vec4 viewPos[4];
};

layout (push_constant) {
};
