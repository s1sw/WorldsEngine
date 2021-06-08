#version 450
#extension GL_EXT_multiview : enable

layout (binding = 0) readonly buffer VB {
    vec2 verts[];
};

layout (push_constant) uniform PC {
    uint eyeOffset;
};

void main() {
    vec2 vPos = verts[gl_VertexIndex + (eyeOffset * gl_ViewIndex)];
    vPos *= 2.0;
    vPos -= 1.0;
    gl_Position = vec4(vPos, 1.0, 1.0);
    gl_Position.y = -gl_Position.y;
}
