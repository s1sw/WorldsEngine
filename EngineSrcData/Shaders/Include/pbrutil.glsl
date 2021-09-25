#ifndef PBRUTIL_HEADER
#define PBRUTIL_HEADER
#include <math.glsl>

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a2 = (roughness * roughness) * (roughness * roughness);
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = max(PI * denom * denom, 0.0001);

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;

    float num = NdotV;
    float denom = max(NdotV * (1.0f - k) + k, 0.0001);

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
float ndfGGX(float cosLh, float roughness) {
    float roughSq = roughness * roughness;
    float alphaSq = roughSq * roughSq;

    float denom = max((cosLh * cosLh) * (alphaSq - 1.0) + 1.0, 0.0001);
    return alphaSq / (PI * denom * denom);
}

float ndfGGXSphereLight(float cosLh, float roughness, float sphereRadius, float lightDist) {
    float alpha = roughness * roughness;
    float alphaPrime = clamp(sphereRadius / (lightDist * 2.0) + alpha, 0.0, 1.0);

    float alphaNotReallySq = alpha * alphaPrime;

    float denom = max((cosLh * cosLh) * (alphaNotReallySq - 1.0) + 1.0, 0.0001);
    return alphaNotReallySq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k) {
    return cosTheta / max(cosTheta * (1.0 - k) + k, 0.0001);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float cosLo, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
    return gaSchlickG1(cosLi, k) * gaSchlickG1(cosLo, k);
}

// Shlick's approximation of the Fresnel factor.
vec3 fresnelSchlick(vec3 F0, float cosTheta) {
    float omCosTheta = 1.0 - cosTheta;
    return F0 + (1.0 - F0) * omCosTheta * omCosTheta * omCosTheta * omCosTheta * omCosTheta;
}

float pow5(float a) {
    return a * a * a * a * a;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow5(1.0f - cosTheta);
}

struct ShadeInfo {
    float metallic;
    float roughness;
    vec3 albedoColor;
    vec3 normal;
    float alphaCutoff;
    vec3 viewDir;
    float ao;
    float alpha;
    vec3 emissive;
};
#endif
