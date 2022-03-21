#version 450
layout (binding = 0, rgba8) uniform writeonly image2D resultImage;
#ifdef MSAA
layout (binding = 1) uniform sampler2DMSArray hdrImage;
#else
layout (binding = 1) uniform sampler2DArray hdrImage;
#endif
layout (binding = 2) uniform sampler2DArray bloomImage;
layout (local_size_x = 16, local_size_y = 16) in;
#include <math.glsl>

#ifdef MSAA
layout(constant_id = 0) const int NUM_MSAA_SAMPLES = 4;
#else
const int NUM_MSAA_SAMPLES = 1;
#endif
layout(push_constant) uniform PushConstants {
    int idx;
    float exposureBias;
    float vignetteRadius;
    float vignetteSoftness;
    vec3 vignetteColor;
    float resolutionScale;
};

float A = 0.15;
float B = 0.50;
float C = 0.10;
float D = 0.20;
float E = 0.02;
float F = 0.30;
float W = 11.2;

vec3 ACESFilm(vec3 x) {
    x *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), vec3(0.0), vec3(1.0));
}

vec3 Uncharted2Tonemap(vec3 x) {
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 InverseTonemap(vec3 x) {
    return ( (x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F) ) - E/F;
}

float rcp(float x) { return 1.0 / x; }

float gmax3(float x, float y, float z) {
    return max(x, max(y, z));
}

// Apply this to tonemap linear HDR color "c" after a sample is fetched in the resolve.
// Note "c" 1.0 maps to the expected limit of low-dynamic-range monitor output.
vec3 Tonemap(vec3 c) { return c * rcp(gmax3(c.r, c.g, c.b) + 1.0); }

// When the filter kernel is a weighted sum of fetched colors,
// it is more optimal to fold the weighting into the tonemap operation.
vec3 TonemapWithWeight(vec3 c, float w) { return c * (w * rcp(gmax3(c.r, c.g, c.b) + 1.0)); }

// Apply this to restore the linear HDR color before writing out the result of the resolve.
vec3 TonemapInvert(vec3 c) { return c * rcp(1.0 - gmax3(c.r, c.g, c.b)); }

vec3 tonemapCol(vec3 col, vec3 whiteScale) {
    col *= 16.0;

    vec3 curr = Uncharted2Tonemap(exposureBias * col);

    return curr * whiteScale;
}

vec2 hdrTexSize() {
#ifdef MSAA
    return vec2(textureSize(hdrImage));
#else
    return vec2(textureSize(hdrImage, 0));
#endif
}

vec2 outSize() {
    return vec2(imageSize(resultImage));
}

vec3 pointSample(vec2 uv, int sampleIdx) {
    vec2 texSize = hdrTexSize();
    
    return texelFetch(hdrImage, ivec3(texSize * uv, idx), sampleIdx).xyz;
}

vec3 bilinearSample(vec2 uv, int sampleIdx) {
    vec2 texelSize = 1.0 / hdrTexSize();
    vec2 tl = uv;
    vec2 tr = uv + vec2(texelSize.x, 0.0);
    vec2 bl = uv + vec2(0.0, texelSize.y);
    vec2 br = uv + texelSize;

    vec3 tls = pointSample(tl, sampleIdx);
    vec3 trs = pointSample(tr, sampleIdx);
    vec3 bls = pointSample(bl, sampleIdx);
    vec3 brs = pointSample(br, sampleIdx);

    vec2 texCoord = fract(hdrTexSize() * uv);

    vec3 topRow = mix(tls, trs, texCoord.x);
    vec3 bottomRow = mix(bls, brs, texCoord.x);

    return mix(topRow, bottomRow, texCoord.y);
}

void main() {
    vec3 acc = vec3(0.0);
    vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
    vec2 uv = (vec2(gl_GlobalInvocationID.xy) + vec2(0.5)) / (hdrTexSize() / resolutionScale);//outSize();

    vec3 bloom = texture(bloomImage, vec3(uv * resolutionScale, idx)).xyz;
    //bloom -= vec3(0.1);
    bloom = max(bloom, vec3(0.0));
    for (int i = 0; i < NUM_MSAA_SAMPLES; i++) {
        //vec3 raw = texelFetch(hdrImage, ivec3(gl_GlobalInvocationID.xy, idx), i).xyz;
        //vec3 raw = texelFetch(hdrImage, ivec3(vec2(gl_GlobalInvocationID.xy) * resolutionScale, idx), i).xyz;
        vec3 raw = resolutionScale == 1.0 ? texelFetch(hdrImage, ivec3(gl_GlobalInvocationID.xy, idx), i).xyz : bilinearSample(uv, i);
        //vec3 raw = bilinearSample(uv, i);
        acc += Tonemap(mix(raw, bloom, 0.2));
        //acc += ACESFilm(raw * 4.0);
    }
    
    acc *= rcp(NUM_MSAA_SAMPLES);
    acc = TonemapInvert(acc);
    acc = saturate(tonemapCol(acc, whiteScale));
    //acc = ACESFilm(acc * exposureBias);
    
    vec2 centerVec = (uv - vec2(0.5, 0.5));
    float smoothness = 0.2;
    float vig = smoothstep(vignetteRadius, vignetteRadius - vignetteSoftness, length(centerVec));
    float vigBlend = smoothstep(vignetteRadius * 2, vignetteRadius * 2 - (vignetteSoftness * 5), length(centerVec));
    
    //// debugging checks for NaN and negatives
    ///*
    //   if (any(lessThan(acc, vec3(0.0)))) acc = vec3(1.0, 0.0, 0.0);
    //   if (any(isnan(acc))) acc = vec3(1.0, 0.0, 1.0);
    // */

    vec3 final = pow(mix(acc, mix(vec3(0.5, 0.0, 0.0), acc, vigBlend), 1.0 - vig), vec3(1 / 2.2));

    imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4(final, 1.0));
}
