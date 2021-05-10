#version 450

layout(binding = 0) uniform MultiVP {
	mat4 view[8];
	mat4 projection[8];
    vec4 viewPos[8];
};

void main() {}
