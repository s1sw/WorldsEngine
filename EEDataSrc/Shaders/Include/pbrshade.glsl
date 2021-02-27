#ifndef PBRSHADE_HEADER
#define PBRSHADE_HEADER
#include "pbrutil.glsl"

struct LightShadeInfo {
    vec3 radiance;
    vec3 L;
};

LightShadeInfo calcLightShadeInfo(Light light, vec3 worldPos) {
    LightShadeInfo lsi;
    lsi.radiance = light.pack0.xyz;
    lsi.L = vec3(0.0f, 0.0f, 0.0f);
    int lightType = int(light.pack0.w);

    if (lightType == LT_POINT) {
        vec3 lightPos = light.pack2.xyz;
        lsi.L = lightPos - worldPos;
        // dot(L, L) = length(L) squared
        lsi.radiance *= 1.0 / dot(lsi.L, lsi.L);
        lsi.L = normalize(lsi.L);
    } else if (lightType == LT_SPOT) {
        vec3 lightPos = light.pack2.xyz;
        lsi.L = normalize(lightPos - worldPos);
        float theta = dot(lsi.L, normalize(light.pack1.xyz));
        float cutoff = light.pack1.w;
        float outerCutoff = cutoff - 0.02f;
        vec3 lToFrag = lightPos - worldPos;
        lsi.radiance *= clamp((theta - outerCutoff) / (cutoff - outerCutoff), 0.0f, 1.0f) * (1.0 / dot(lToFrag, lToFrag));
    } else {
        lsi.L = normalize(light.pack1.xyz);
    }

    return lsi;
}

vec3 calculateLighting(Light light, ShadeInfo shadeInfo, vec3 worldPos) {
    LightShadeInfo lsi = calcLightShadeInfo(light, worldPos);

    vec3 halfway = normalize(shadeInfo.viewDir + lsi.L);
    vec3 norm = shadeInfo.normal;
    float cosLh = max(0.0f, dot(norm, halfway));
    float cosLi = max(0.0f, dot(norm, lsi.L));

#ifdef BLINN_PHONG
    float specIntensity = pow(cosLh, (1.0 / max(roughness, 0.001)) * 50.0) * (1.0 - (roughness * roughness));
    return (vec3(specIntensity) * radiance) + (1.0 - metallic) * (albedoColor * radiance * cosLi);
#else
    float cosLo = max(0.0f, dot(norm, shadeInfo.viewDir));

    float NDF = ndfGGX(cosLh, shadeInfo.roughness);
    float G = gaSchlickGGX(cosLi, cosLo, shadeInfo.roughness);
    vec3 f = fresnelSchlick(shadeInfo.f0, max(dot(norm, shadeInfo.viewDir), 0.0f));

    vec3 kd = mix(vec3(1.0) - f, vec3(0.0), shadeInfo.metallic);

    vec3 numerator = NDF * G * f;
    float denominator = 4.0f * cosLo * cosLi;
    vec3 specular = numerator / max(denominator, 0.001f);

    return (specular + (kd * shadeInfo.albedoColor)) * lsi.radiance * cosLi;
#endif
}
#endif
