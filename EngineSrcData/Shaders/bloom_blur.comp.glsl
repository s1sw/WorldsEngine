#version 450
layout (local_size_x = 16, local_size_y = 16) in;
#ifdef SEED
layout (binding = 0) uniform sampler2DMSArray inputTexture;
#else
layout (binding = 0) uniform sampler2D inputTexture;
#endif
layout (binding = 1, rgba16f) uniform writeonly image2D outputTexture;
#include <math.glsl>
#include <inversible_tonemap.glsl>

// Simple box filter

layout (push_constant) uniform PC {
    uint inputMipLevel;
    uint inputArrayIdx;
    int numMsaaSamples;
};

vec4 samp(vec2 uv) {
    vec2 coords = vec2(uv);
#ifdef SEED
    int nSamples = numMsaaSamples;
    vec3 acc = vec3(0.0);
    for (int i = 0; i < nSamples; i++) {
        vec3 sampl = texelFetch(inputTexture, ivec3(gl_GlobalInvocationID.xy, inputArrayIdx), i).xyz;
        acc += Tonemap(sampl);
    }
    acc /= float(nSamples);
    acc = TonemapInvert(acc);
    return vec4(acc, 0.0);

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

vec4 downsample13(vec2 uv, vec2 resolution) {
    vec2 texelSize = 2.0 / resolution;
    
    // A  B  C
    //  D  E
    // F  G  H
    //  I  J
    // K  L  M
    
    // Samples going clockwise from the top left corner
    
    vec4 a = samp(uv - vec2(-1.0,  1.0) * texelSize);
    vec4 b = samp(uv - vec2( 0.0,  1.0) * texelSize);
    vec4 c = samp(uv - vec2( 1.0,  1.0) * texelSize);
    
    vec4 d = samp(uv - vec2(-0.5,  0.5) * texelSize);
    vec4 e = samp(uv - vec2( 0.5,  0.5) * texelSize);
    
    vec4 f = samp(uv - vec2(-1.0,  0.0) * texelSize);
    vec4 g = samp(uv - vec2( 0.0,  0.0) * texelSize);
    vec4 h = samp(uv - vec2( 1.0,  0.0) * texelSize);
    
    vec4 i = samp(uv - vec2(-0.5, -0.5) * texelSize);
    vec4 j = samp(uv - vec2( 0.5, -0.5) * texelSize);
    
    vec4 k = samp(uv - vec2(-1.0, -1.0) * texelSize);
    vec4 l = samp(uv - vec2( 0.0, -1.0) * texelSize);
    vec4 m = samp(uv - vec2( 1.0, -1.0) * texelSize);
    
    float sampleWeight = 0.25;
    
    // slides 154-158
    vec4 final =
        (d + e + i + j) * sampleWeight * 0.5
      + (a + b + f + g) * sampleWeight * 0.125
      + (b + c + g + h) * sampleWeight * 0.125
      + (g + h + l + m) * sampleWeight * 0.125
      + (f + g + k + l) * sampleWeight * 0.125;

    return final;
}

vec4 downsample4(vec2 uv, vec2 resolution) {
    vec2 texelSize = 1.0 / resolution;

    vec4 final =
        samp(uv + vec2(1.0, 1.0) * texelSize) +
        samp(uv + vec2(1.0, -1.0) * texelSize) +
        samp(uv + vec2(-1.0, -1.0) * texelSize) +
        samp(uv + vec2(-1.0, 1.0) * texelSize);
    
    final *= 0.25;

    return final;
}

vec4 tent(vec2 uv, vec2 resolution) {
    const float radius = 1.0;
    vec2 xOffset = vec2(1.0 / resolution.x, 0.0) * radius;
    vec2 yOffset = vec2(0.0, 1.0 / resolution.y) * radius;
    vec4 color = vec4(0.0);
    
    color += samp(uv) * 4.0;

    color += samp(uv + xOffset) * 2.0;
    color += samp(uv - xOffset) * 2.0;

    color += samp(uv + yOffset) * 2.0;
    color += samp(uv - yOffset) * 2.0;

    color += samp(uv + xOffset + yOffset);
    color += samp(uv + xOffset - yOffset);
    color += samp(uv - xOffset + yOffset);
    color += samp(uv - xOffset - yOffset);
    
    color /= 16.0;

    return color;
}

vec4 upsampleLq(vec2 uv, vec2 resolution) {
    const float radius = 1.0;
    vec2 offset = 0.5 / resolution;
    vec4 color = vec4(0.0);

    color += samp(uv + vec2(1.0, 1.0) * offset);
    color += samp(uv + vec2(-1.0, 1.0) * offset);
    color += samp(uv + vec2(-1.0, -1.0) * offset);
    color += samp(uv + vec2(1.0, -1.0) * offset);
    
    color /= 4.0;

    return color;
}

//#define LOW_QUALITY

void main() {
    uvec2 resolution = imageSize(outputTexture).xy;
    vec2 uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / vec2(resolution);

#ifndef SEED
    vec4 blurred = vec4(0.0);
#ifndef UPSAMPLE
#ifdef LOW_QUALITY
    blurred = downsample4(uv, resolution);
#else
    blurred = downsample13(uv, resolution);
#endif
#else
    vec4 orig = textureLod(inputTexture, uv, inputMipLevel - 1);
#ifdef LOW_QUALITY
    blurred = upsampleLq(uv, resolution);
#else
    blurred = tent(uv, resolution);
#endif
    //if (length(blurred) < 1.1) blurred = orig;
    blurred = mix(orig, blurred, 0.5);
    //blurred += orig;
#endif
    imageStore(outputTexture, ivec2(gl_GlobalInvocationID.xy), vec4(blurred.xyz, 1.0f));
#else
    vec3 col = samp(uv).xyz;
    //col = QuadraticThreshold(col, 12.5, 0.5);
    col = max(col, vec3(0.0));
    //col = saturate(col);
    imageStore(outputTexture, ivec2(gl_GlobalInvocationID.xy), vec4(col, 1.0));
#endif
}
