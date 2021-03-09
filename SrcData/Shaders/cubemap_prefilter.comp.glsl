#version 450
const float PI = 3.141592654;
layout (local_size_x = 16, local_size_y = 16) in;

layout (push_constant) uniform PC {
    float roughness;
    int faceIdx;
};

layout (binding = 0, rgba32f) uniform readonly image2D inFace;
layout (binding = 1, rgba32f) uniform writeonly image2D outFace;
layout (binding = 2) uniform samplerCube fullCubemap;

// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// efficient VanDerCorpus calculation.
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
vec2 Hammersley(uint i, uint N) {
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}
// ----------------------------------------------------------------------------
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness*roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);

    // from spherical coordinates to cartesian coordinates - halfway vector
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent-space H vector to world-space sample vector
    vec3 up          = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

vec3 getSamplingVector(uvec2 pixCoord) {
    ivec2 texSize = imageSize(outFace);

    vec2 st = vec2(pixCoord) / vec2(texSize);
    vec2 uv = 2.0 * vec2(st.x, 1.0 - st.y) - 1.0;

    // Select vector based on cubemap face index.
    vec3 ret;
    switch (faceIdx) {
        case 0:
            ret = vec3(1.0, uv.y, -uv.x);
            break;
        case 1:
            ret = vec3(-1.0, uv.y, uv.x);
            break;
        case 2:
            ret = vec3(uv.x, 1.0, -uv.y);
            break;
        case 3:
            ret = vec3(uv.x, -1.0, uv.y);
            break;
        case 4:
            ret = vec3(uv.x, uv.y, 1.0);
            break;
        case 5:
            ret = vec3(-uv.x, uv.y, -1.0);
            break;
    }
    return normalize(ret);
}

void main() {
    uvec2 texSize = imageSize(outFace);
    uvec2 inSize = imageSize(inFace);
    if (any(greaterThan(gl_GlobalInvocationID.xy, texSize))) {
        return;
    }

    vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(texSize);
    ivec2 inCoords = ivec2(vec2(inSize) * uv);

    vec3 N = normalize(getSamplingVector(gl_GlobalInvocationID.xy));
    vec3 R = N;
    vec3 V = R;

    float totalWeight = 0.0f;
    vec3 prefilteredColor = vec3(0.0f, 0.0f, 0.0f);

    const uint SAMPLE_COUNT = 768;
    for (uint i = 0u; i < SAMPLE_COUNT; i++) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0f);
        if (NdotL > 0.0f) {
            prefilteredColor += clamp(textureLod(fullCubemap, L, 0.0).xyz, vec3(0.0), vec3(5.0)) * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / totalWeight;

    imageStore(outFace, ivec2(gl_GlobalInvocationID.xy), vec4(prefilteredColor, 1.0f));
}
