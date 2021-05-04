#ifndef PBRSHADE_HEADER
#define PBRSHADE_HEADER
#include "pbrutil.glsl"

struct LightShadeInfo {
    vec3 radiance;
    vec3 L;
    float lightDist;
};

float length2(vec3 v) {
    return dot(v, v);
}

LightShadeInfo calcLightShadeInfo(Light light, ShadeInfo shadeInfo, vec3 worldPos) {
    LightShadeInfo lsi;
    lsi.radiance = light.pack0.xyz;
    lsi.L = vec3(0.0f, 0.0f, 0.0f);
    int lightType = int(light.pack0.w);

    if (lightType == LT_DIRECTIONAL) {
        lsi.L = normalize(light.pack1.xyz);
    } else if (lightType == LT_POINT) {
        vec3 lightPos = light.pack2.xyz;
        lsi.L = lightPos - worldPos;

        // dot(L, L) = length(L) squared
        lsi.radiance *= 1.0 / length2(lsi.L);
        lsi.radiance *= length2(lsi.radiance) < 0.02 * 0.02 ? 0.0 : 1.0;

        lsi.L = normalize(lsi.L);
    } else if (lightType == LT_SPOT) {
        float cutoff = light.pack1.w;
        float outerCutoff = cutoff - 0.02f;
        vec3 lightPos = light.pack2.xyz;

        lsi.L = normalize(lightPos - worldPos);

        float theta = dot(lsi.L, normalize(light.pack1.xyz));
        vec3 lToFrag = lightPos - worldPos;
        lsi.radiance *= clamp((theta - outerCutoff) / (cutoff - outerCutoff), 0.0f, 1.0f) * (1.0 / dot(lToFrag, lToFrag));

        lsi.radiance *= length2(lsi.radiance) < 0.02 * 0.02 ? 0.0 : 1.0;
    } else if (lightType == LT_SPHERE) {
        vec3 lightPos = light.pack2.xyz;
        float sphereRadius = light.pack1.w;
        float sphereRadiusSq = sphereRadius * sphereRadius;

        vec3 r = reflect(-shadeInfo.viewDir, shadeInfo.normal);
        lsi.L = lightPos - worldPos;
        vec3 centerToRay = (dot(lsi.L, r) * r) - lsi.L;
        vec3 closestPoint = lsi.L + centerToRay * saturate(sphereRadius / length(centerToRay));
        lsi.L = normalize(closestPoint);
        float lightDist = length(closestPoint);
        float sqrDist = lightDist * lightDist;
        float falloff = (sphereRadiusSq / ( max(sphereRadiusSq, sqrDist)));

        lsi.radiance *= falloff;
        lsi.lightDist = lightDist;
    } else if (lightType == LT_TUBE) {
        vec3 p0 = light.pack1.xyz;
        vec3 p1 = light.pack2.xyz;
        vec3 r = reflect(-shadeInfo.viewDir, shadeInfo.normal);
        float tubeRadius = light.pack1.w;

        vec3 l0 = p0 - worldPos;
        vec3 l1 = p1 - worldPos;

        float distL0 = length( l0 );
        float distL1 = length( l1 );
        vec3 Ldist = l1 - l0;
        float RoLd = dot( r, Ldist);
        float distLd = length(Ldist);
        float t = ( dot( r, l0 ) * RoLd - dot( l0, Ldist) ) / ( distLd * distLd - RoLd * RoLd );

        vec3 closestPoint = l0 + Ldist * saturate(t);
        vec3 centerToRay = dot(closestPoint, r) * r - closestPoint;
        closestPoint = closestPoint + centerToRay * saturate(tubeRadius / length(centerToRay));
        vec3 L = normalize(closestPoint);
        float distLight = length(closestPoint);

        lsi.L = L;
        float falloff = (tubeRadius * tubeRadius / max(tubeRadius * tubeRadius, distLight * distLight));
        lsi.radiance *= falloff;
        lsi.lightDist = distLight;
    }

    return lsi;
}

vec3 calculateLighting(Light light, ShadeInfo shadeInfo, vec3 worldPos) {
    LightShadeInfo lsi = calcLightShadeInfo(light, shadeInfo, worldPos);

    vec3 halfway = normalize(shadeInfo.viewDir + lsi.L);
    vec3 norm = shadeInfo.normal;
    float cosLh = max(0.0f, dot(norm, halfway));
    float cosLi = max(0.0f, dot(norm, lsi.L));

#ifdef BLINN_PHONG
    float roughness = shadeInfo.roughness;
    float metallic = shadeInfo.metallic;
    vec3 albedoCol = shadeInfo.albedoColor;
    float specIntensity = pow(cosLh, (1.0 / max(roughness, 0.001)) * 50.0) * (1.0 - (roughness * roughness));
    return (vec3(specIntensity) * lsi.radiance) + (1.0 - metallic) * (albedoCol * lsi.radiance * cosLi);
#else
    float cosLo = max(0.0f, dot(norm, shadeInfo.viewDir));

    float NDF;
    int lType = int(light.pack0.w);

    if (lType != LT_SPHERE && lType != LT_TUBE) {
        NDF = ndfGGX(cosLh, shadeInfo.roughness);
    } else {
        NDF = ndfGGXSphereLight(cosLh, shadeInfo.roughness, light.pack2.w, lsi.lightDist);
    }

    float G = gaSchlickGGX(cosLi, cosLo, shadeInfo.roughness);
    vec3 f = fresnelSchlick(shadeInfo.f0, max(dot(norm, shadeInfo.viewDir), 0.0f));

    vec3 kd = mix(vec3(1.0) - f, vec3(0.0), shadeInfo.metallic);

    vec3 numerator = NDF * G * f;
    float denominator = 4.0f * cosLo * cosLi;
    vec3 specular = numerator / max(denominator, 0.001f);
    vec3 diffuse = kd * shadeInfo.albedoColor;

    return (specular + diffuse) * lsi.radiance * cosLi;
#endif
}
#endif
