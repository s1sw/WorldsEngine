#version 450
layout (local_size_x = 16, local_size_y = 16) in;
#ifdef SEED
layout (binding = 0) uniform sampler2DMSArray inputTexture;
#else
layout (binding = 0) uniform sampler2D inputTexture;
#endif
layout (binding = 1, rgba16f) uniform writeonly image2D outputTexture;
#include <math.glsl>

// Simple box filter

layout (push_constant) uniform PC {
    vec2 direction;
    uint inputMipLevel;
    float resScalar;
    uvec2 resolution;
};

vec4 samp(vec2 uv) {
    vec2 coords = vec2(uv);
#ifdef SEED
    return texelFetch(inputTexture, ivec3(gl_GlobalInvocationID.xy, 0), 0);
#else
    return textureLod(inputTexture, coords, inputMipLevel);
#endif
}

vec4 blur13(vec2 uv, vec2 resolution, vec2 direction) {
    vec4 color = vec4(0.0);
    vec2 off1 = vec2(1.411764705882353) * direction;
    vec2 off2 = vec2(3.2941176470588234) * direction;
    vec2 off3 = vec2(5.176470588235294) * direction;
    color += samp(uv) * 0.1964825501511404;
    color += samp(uv + (off1 / resolution)) * 0.2969069646728344;
    color += samp(uv - (off1 / resolution)) * 0.2969069646728344;
    color += samp(uv + (off2 / resolution)) * 0.09447039785044732;
    color += samp(uv - (off2 / resolution)) * 0.09447039785044732;
    color += samp(uv + (off3 / resolution)) * 0.010381362401148057;
    color += samp(uv - (off3 / resolution)) * 0.010381362401148057;
    return color;
}

vec4 blur9(vec2 uv, vec2 resolution, vec2 direction) {
    vec4 color = vec4(0.0);
    vec2 off1 = vec2(1.3846153846) * direction;
    vec2 off2 = vec2(3.2307692308) * direction;
    color += samp(uv) * 0.2270270270;
    color += samp(uv + (off1 / resolution)) * 0.3162162162;
    color += samp(uv - (off1 / resolution)) * 0.3162162162;
    color += samp(uv + (off2 / resolution)) * 0.0702702703;
    color += samp(uv - (off2 / resolution)) * 0.0702702703;
    return color;
}

vec4 blur5(vec2 uv, vec2 resolution, vec2 direction) {
    vec4 color = vec4(0.0);
    vec2 off1 = vec2(1.3333333333333333) * direction;
    color += samp(uv) * 0.29411764705882354;
    color += samp(uv + (off1 / resolution)) * 0.35294117647058826;
    color += samp(uv - (off1 / resolution)) * 0.35294117647058826;
    return color;
}

void main() {
    //vec2 resolution = textureSize(inputTexture, inputMipLevel).xy;
    vec2 uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / vec2(resolution * resScalar);

#ifndef SEED
    vec4 blurred = blur9(uv, resolution * resScalar, direction);
    imageStore(outputTexture, ivec2(gl_GlobalInvocationID.xy), vec4(blurred.xyz, 1.0f));
#else
    vec3 col = samp(uv).xyz;
    col -= 1.0;
    col = saturate(col);
    imageStore(outputTexture, ivec2(gl_GlobalInvocationID.xy), vec4(col, 1.0));
#endif
}
