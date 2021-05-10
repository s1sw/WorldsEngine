#version 450
#extension GL_EXT_multiview : enable
#include <shadercomms.glsl>

layout(location = 0) VARYING(vec2, UV);

#ifdef VERTEX
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
#endif

layout(binding = 0) uniform MultiVP {
    mat4 view[2];
    mat4 projection[2];
    vec4 viewPos[2];
};

layout (push_constant) uniform PushConstants {
    uint vpIdx;
    uint texIdx;
    uint pad0;
    uint pad1;
};

#ifdef VERTEX
void main() {
    uint vpIdx = vpIdx + gl_ViewIndex;

}
#endif

#ifdef FRAGMENT
void main() {
}
#endif
