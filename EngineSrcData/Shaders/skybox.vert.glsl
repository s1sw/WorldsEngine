#version 450
#extension GL_EXT_multiview : enable

//layout (location = 0) in vec3 inPos;

layout (location = 0) out vec3 outTexCoords;

layout (binding = 0) uniform MultiVP {
    mat4 view[2];
    mat4 projection[2];
    vec4 viewPos[2];
};

layout (push_constant) uniform PushConstants {
    // (x: vp index, y: cubemap index)
    ivec4 ubIndices;
};

void main() {

    int tri = gl_VertexIndex / 3;
    int idx = gl_VertexIndex % 3;
    int face = tri / 2;
    int top = tri % 2;

    int dir = face % 3;
    int pos = face / 3;

    int nz = dir >> 1;
    int ny = dir & 1;
    int nx = 1 ^ (ny | nz);

    vec3 d = vec3(nx, ny, nz);
    float flip = 1 - 2 * pos;

    vec3 n = flip * d;
    vec3 u = -d.yzx;
    vec3 v = flip * d.zxy;

    float mirror = -1 + 2 * top;
    vec3 xyz = n + mirror*(1-2*(idx&1))*u + mirror*(1-2*(idx>>1))*v;

    uint vpIdx = ubIndices.y;

#ifndef AMD_VIEWINDEX_WORKAROUND
    vpIdx += gl_ViewIndex;
#endif

    vec4 transformedPos = projection[vpIdx] * mat4(mat3(view[vpIdx])) * vec4(xyz, 1.0); // Apply MVP transform
    gl_Position = transformedPos.xyww;
    gl_Position.z = 0.0;
    gl_Position.y = - gl_Position.y;
    xyz.x = -xyz.x;
    outTexCoords = xyz;
}