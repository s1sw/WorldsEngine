#version 450
#extension GL_EXT_scalar_block_layout : enable
layout (local_size_x = 64, local_size_y = 1) in;
struct Vertex {
    vec3 pos;
    vec3 normal;
    vec3 tangent;
    vec2 uv;
    vec2 uv2;
};

layout (scalar, binding = 0) buffer SourceVB {
    Vertex unskinnedVerts[];
};

layout (scalar, binding = 1) buffer SkinnedVB {
    Vertex skinnedVerts[];
};

struct VertSkinInfo {
    ivec4 indices;
    vec4 weights;
};

layout (scalar, binding = 2) buffer VertSkinInfoBuf {
    VertSkinInfo[] skinInfo;
};

layout (scalar, binding = 3) buffer BoneBuf {
    mat4x4 poses[];
};

layout (std140, push_constant) uniform PC {
    uint numVertices;
};

void main() {
    if (gl_GlobalInvocationID.x >= numVertices) return;
    uint idx = gl_GlobalInvocationID.x;

    float weightSum = 0.0;

    Vertex vert = unskinnedVerts[idx];
    VertSkinInfo vInfo = skinInfo[idx];
    vec3 pos = vec3(0.0);
    vec3 norm = vec3(0.0);
    vec3 tangent = vec3(0.0);

    for (int i = 0; i < 4; i++) {
        if (weightSum >= 1.0f) break;

        float weight = vInfo.weights[i];
        mat4x4 mat = poses[vInfo.indices[i]];
        pos += (mat * vec4(vert.pos, 1.0)).xyz * weight;
        norm += (mat * vec4(vert.normal, 0.0)).xyz * weight;
        tangent += (mat * vec4(vert.tangent, 0.0)).xyz * weight;
    }

    skinnedVerts[idx] = vert;
    skinnedVerts[idx].pos = pos;
    skinnedVerts[idx].normal = norm;
    skinnedVerts[idx].tangent = tangent;
}
