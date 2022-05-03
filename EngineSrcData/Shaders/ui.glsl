#version 450
#extension GL_EXT_multiview : enable
#extension GL_EXT_nonuniform_qualifier : require
#include <shadercomms.glsl>

layout(location = 0) VARYING(vec2, UV);

#ifdef VERTEX
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
#endif

#ifdef FRAGMENT
layout(location = 0) out vec4 FragColor;
#endif

layout(binding = 0) uniform MultiVP {
    mat4 view[2];
    mat4 projection[2];
    mat4 inverseVP[2];
    vec4 viewPos[2];
};

layout(binding = 1) uniform sampler2D tex2dSampler[];

layout (push_constant) uniform PushConstants {
    uint vpIdx;
    uint texIdx;
    uint pad0;
    uint pad1;
};

#ifdef VERTEX
void main() {
    uint vpIdx = vpIdx + gl_ViewIndex;
    gl_Position = projection[vpIdx] * view[vpIdx] * vec4(inPosition, 1.0);
}
#endif

#ifdef FRAGMENT
void main() {
    FragColor = texture(tex2dSampler[texIdx], inUV);
}
#endif
