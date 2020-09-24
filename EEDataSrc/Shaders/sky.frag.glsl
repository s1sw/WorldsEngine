#version 450
layout (location = 0) in vec3 pos;
layout (binding = 0) uniform samplerCube cubemap;
layout (location = 0) out vec4 col;

void main() {
    col = texture(cubemap, pos);
}