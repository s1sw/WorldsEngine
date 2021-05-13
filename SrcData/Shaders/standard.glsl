#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_multiview : enable
#define MAX_SHADOW_LIGHTS 16
#define HIGH_QUALITY_SHADOWS
#include <math.glsl>
#include <light.glsl>
#include <material.glsl>
#include <pbrutil.glsl>
#include <pbrshade.glsl>
#include <parallax.glsl>
#include <shadercomms.glsl>
#include <aobox.glsl>

layout(location = 0) VARYING(vec4, WorldPos);
layout(location = 1) VARYING(vec3, Normal);
layout(location = 2) VARYING(vec3, Tangent);
layout(location = 3) VARYING(vec2, UV);
layout(location = 4) VARYING(float, Depth);
layout(location = 5) VARYING(flat uint, UvDir);

#ifdef FRAGMENT
#ifdef EFT
layout(early_fragment_tests) in;
#endif
layout(location = 0) out vec4 FragColor;

layout(constant_id = 0) const bool ENABLE_PICKING = false;
layout(constant_id = 1) const float PARALLAX_MAX_LAYERS = 32.0;
layout(constant_id = 2) const float PARALLAX_MIN_LAYERS = 4.0;
layout(constant_id = 3) const bool DO_PARALLAX = false;
#endif

#ifdef VERTEX
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUV;
#endif

#include <standard_descriptors.glsl>
#include <standard_push_constants.glsl>

#ifdef VERTEX
void main() {
    mat4 model = modelMatrices[modelMatrixIdx + gl_InstanceIndex];

    int vpMatIdx = vpIdx + gl_ViewIndex;
    outWorldPos = (model * vec4(inPosition, 1.0));

    mat4 projMat = projection[vpMatIdx];

    gl_Position = projection[vpMatIdx] * view[vpMatIdx] * model * vec4(inPosition, 1.0); // Apply MVP transform

    mat3 model3 = mat3(model);
    // remove scaling
    model3[0] = normalize(model3[0]);
    model3[1] = normalize(model3[1]);
    model3[2] = normalize(model3[2]);
    outNormal = normalize(model3 * inNormal);
    outTangent = normalize(model3 * inTangent);
    outDepth = gl_Position.z / gl_Position.w;
    //outViewPos = -(view[vpMatIdx] * model * vec4(inPosition, 1.0)).xyz;
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness

    vec2 uv = inUV;
    outUvDir = 0;

    if ((miscFlag & 128) == 128) {
        uv = outWorldPos.xy;
        outUvDir = 1;
    } else if ((miscFlag & 256) == 256) {
        uv = outWorldPos.xz;
        outUvDir = 2;
    } else if ((miscFlag & 512) == 512) {
        uv = outWorldPos.zy;
        outUvDir = 3;
    } else if ((miscFlag & 1024) == 1024) {
        // Find maximum axis
        uint maxAxis = 0;

        vec3 dots = vec3(0.0);

        dots.x = abs(dot(outNormal, vec3(1.0, 0.0, 0.0)));
        dots.y = abs(dot(outNormal, vec3(0.0, 1.0, 0.0)));
        dots.z = abs(dot(outNormal, vec3(0.0, 0.0, 1.0)));
        float maxProduct = max(dots.x, max(dots.y, dots.z));

        // Assume flat surface for tangents
        if (dots.x == maxProduct) {
            uv = outWorldPos.zy;
            outUvDir = 1;
        } else if (dots.y == maxProduct) {
            uv = outWorldPos.xz;
            outUvDir = 2;
        } else {
            uv = outWorldPos.xy;
            outUvDir = 3;
        }
    }

    outUV = (uv * texScaleOffset.xy) + texScaleOffset.zw;
}
#endif

#ifdef FRAGMENT
vec3 parallaxCorrect(vec3 v, vec3 center, vec3 ma, vec3 mi, vec3 pos) {
    vec3 dir = normalize(v);
    vec3 rbmax = (ma - pos) / dir;
    vec3 rbmin = (mi - pos) / dir;

    vec3 rbminmax = max(rbmax, rbmin);

    float dist = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    vec3 intersection = pos + dir * dist;

    return intersection - center;
}

vec3 calcAmbient(vec3 f0, float roughness, vec3 viewDir, float metallic, vec3 albedoColor, vec3 normal) {

    const float MAX_REFLECTION_LOD = 6.0;
    vec3 R = reflect(-viewDir, normal);

    vec3 F = fresnelSchlickRoughness(clamp(dot(normal, viewDir), 0.0, 1.0), f0, roughness);

    if ((miscFlag & 4096) == 4096) {
        R = parallaxCorrect(R, cubemapPos, cubemapPos + cubemapExt, cubemapPos - cubemapExt, inWorldPos.xyz);
    }
    R.x = -R.x;
    R = normalize(R);

    vec3 specularAmbient = textureLod(cubemapSampler[cubemapIdx], R, roughness * MAX_REFLECTION_LOD).rgb;

    vec2 coord = vec2(roughness, max(dot(normal, viewDir), 0.0));

    vec2 brdf = textureLod(brdfLutSampler, coord, 0.0).rg;

    vec3 specularColor = (F * (brdf.x + brdf.y));

    float horizon = min(1.0 + dot(R, normal), 1.0);
    vec3 diffuseAmbient = textureLod(cubemapSampler[cubemapIdx], normal, 7.0).xyz * albedoColor;

    vec3 kD = (1.0 - F) * (1.0 - metallic);

    return kD * diffuseAmbient + (specularAmbient * specularColor);
}

vec3 calcAmbientMetallic(vec3 f0, float roughness, vec3 viewDir, vec3 albedoColor, vec3 normal) {
    const float MAX_REFLECTION_LOD = 6.0;
    vec3 R = reflect(-viewDir, normal);

    vec3 F = fresnelSchlickRoughness(clamp(dot(normal, viewDir), 0.0, 1.0), f0, roughness);

    if ((miscFlag & 4096) == 4096) {
        R = parallaxCorrect(R, cubemapPos, cubemapPos + cubemapExt, cubemapPos - cubemapExt, inWorldPos.xyz);
    }
    R.x = -R.x;
    R = normalize(R);

    vec3 specularAmbient = textureLod(cubemapSampler[cubemapIdx], R, roughness * MAX_REFLECTION_LOD).rgb;

    vec2 coord = vec2(roughness, max(dot(normal, viewDir), 0.0));

    vec2 brdf = textureLod(brdfLutSampler, coord, 0.0).rg;

    vec3 specularColor = (F * (brdf.x + brdf.y));

    float horizon = min(1.0 + dot(R, normal), 1.0);

    return (specularAmbient * specularColor);
}

vec3 decodeNormal (vec2 texVal) {
    vec3 n;
    n.xy = (texVal*2.0)-1.0;
    vec2 xySq = n.xy * n.xy;
    n.z = max(sqrt(1.0 - xySq.x - xySq.y), 0.0);
    return n;
}

vec3 getNormalMapNormal(Material mat, vec2 tCoord, mat3 tbn) {
    vec3 texNorm = decodeNormal(texture(tex2dSampler[mat.normalTexIdx], tCoord).xy);
    return tbn * texNorm;
}


void handleEditorPicking() {
    if (pixelPickCoords == ivec2(gl_FragCoord.xy)) {
        pickBuf.objectID = objectId;
    }
}

float mipMapLevel() {
    return textureQueryLod(tex2dSampler[materials[matIdx].albedoTexIdx], inUV).x;
}

bool isTextureEnough(ivec2 texSize) {
    vec2  dx_vtc        = dFdx(inUV);
    vec2  dy_vtc        = dFdy(inUV);
    vec2 d = max(dx_vtc, dy_vtc);

    return all(greaterThan(d * vec2(texSize), vec2(4.0)));
}

int calculateCascade(out vec4 oShadowPos) {
    for (int i = 0; i < 3; i++) {
        vec4 shadowPos = dirShadowMatrices[i] * inWorldPos;
        shadowPos.y = -shadowPos.y;
        vec2 coord = (shadowPos.xy * 0.5 + 0.5);
        const float lThresh = (1.0 / pack0[i + 1]);
        const float hThresh = 1.0 - lThresh;

        if (coord.x > lThresh && coord.x < hThresh &&
                coord.y > lThresh && coord.y < hThresh) {
            oShadowPos = shadowPos;
            return i;
        }
    }
    return 2;
}

float calcProxyAO(vec3 wPos, vec3 normal) {
    float proxyAO = 1.0;

    for (int i = 0; i < int(pack1.x); i++) {
        if (floatBitsToUint(aoBox[i].pack3.w) != objectId) {
            proxyAO *= (1.0 - getBoxOcclusion(aoBox[i], inWorldPos.xyz, normal));
        }
    }

    return proxyAO;
}

float getDirLightShadowIntensity(int lightIdx) {
    vec4 shadowPos;
    float shadowIntensity = 1.0;
    int cascadeSplit = calculateCascade(shadowPos);

    float bias = max(0.0004 * (1.0 - dot(inNormal, lights[lightIdx].pack1.xyz)), 0.00025);
    //float bias = 0.000325;
    float depth = (shadowPos.z / shadowPos.w) - bias;
    vec2 coord = (shadowPos.xy * 0.5 + 0.5);

    if (coord.x > 0.0 && coord.x < 1.0 &&
            coord.y > 0.0 && coord.y < 1.0 &&
            depth < 1.0 && depth > 0.0) {
        float texelSize = 1.0 / textureSize(shadowSampler, 0).x;
        shadowIntensity = 0.0;
#ifdef HIGH_QUALITY_SHADOWS
        const int shadowSamples = 1;
        const float divVal = 4.0f;//((shadowSamples * 2)) * ((shadowSamples * 2));
        float sampleRadius = 0.0005 * (textureSize(shadowSampler, 0).x / 1024.0);

        //for (int x = -shadowSamples; x < shadowSamples; x++)
        //    for (int y = -shadowSamples; y < shadowSamples; y++) {
        //        //shadowIntensity += texture(shadowSampler, vec4(coord + (vec2(x, y) * sampleRadius), float(cascadeSplit), depth)).x;
        //        shadowIntensity += textureOffset(shadowSampler, vec4(coord, cascadeSplit, depth), ivec2(x, y));
        //    }
        vec4 sampleCoord = vec4(coord, cascadeSplit, depth);
        shadowIntensity += textureOffset(shadowSampler, sampleCoord, ivec2(-1, -1));
        shadowIntensity += textureOffset(shadowSampler, sampleCoord, ivec2(-1, 0));

        shadowIntensity += textureOffset(shadowSampler, sampleCoord, ivec2(0, -1));
        shadowIntensity += textureOffset(shadowSampler, sampleCoord, ivec2(0, 0));

        shadowIntensity /= divVal;
#else
        shadowIntensity = texture(shadowSampler, vec4(coord, float(cascadeSplit), depth)).x;
#endif
    }
    return shadowIntensity;
}

vec3 shade(ShadeInfo si) {
    int lightCount = int(pack0.x);

    vec3 ambient;

    vec3 lo = vec3(0.0);
    if (si.metallic == 1.0) {
        for (int i = 0; i < lightCount; i++) {
            vec3 l = calculateLightingMetallic(lights[i], si, inWorldPos.xyz);
            float shadowIntensity = 1.0;
            if (int(lights[i].pack0.w) == LT_DIRECTIONAL && !((miscFlag & 16384) == 16384)) {
                shadowIntensity = getDirLightShadowIntensity(i);
            }
            lo += l * shadowIntensity;
        }

        ambient = calcAmbientMetallic(si.f0, si.roughness, si.viewDir, si.albedoColor, si.normal);
    } else {
        for (int i = 0; i < lightCount; i++) {
            vec3 l = calculateLighting(lights[i], si, inWorldPos.xyz);
            float shadowIntensity = 1.0;
            if (int(lights[i].pack0.w) == LT_DIRECTIONAL && !((miscFlag & 16384) == 16384)) {
                shadowIntensity = getDirLightShadowIntensity(i);
            }
            lo += l * shadowIntensity;
        }
        ambient = calcAmbient(si.f0, si.roughness, si.viewDir, si.metallic, si.albedoColor, si.normal);
    }

    return lo + ambient * si.ao;
}

float getAntiAliasedRoughness(float inRoughness, vec3 normal) {
    vec3 ddxN = dFdx(normal);
    vec3 ddyN = dFdy(normal);

    float geoRoughness = pow(saturate(max(dot(ddxN.xyz, ddxN.xyz), dot(ddyN.xyz, ddyN.xyz))), 0.333);
    return max(inRoughness, geoRoughness);
}

void main() {
    Material mat = materials[matIdx];

    float alphaCutoff = (mat.cutoffFlags & (0xFF)) / 255.0f;
    uint flags = (mat.cutoffFlags & (0x7FFFFF80)) >> 8;

#ifdef MIP_MAP_DBG
    float mipLevel = min(3, mipMapLevel());

    FragColor = vec4(vec3(mipLevel < 0.1, mipLevel * 0.5, mipLevel * 0.25), 0.0);
    FragColor = max(FragColor, vec4(0.0));
    return;
#endif

#ifdef TEXRES_DBG
    bool ite = isTextureEnough(textureSize(tex2dSampler[mat.albedoTexIdx], 0));
    if (!ite) {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }
#endif

    vec3 viewDir = normalize(viewPos[gl_ViewIndex].xyz - inWorldPos.xyz);

    // I'm going to be honest: I have no clue what the fuck is happening here.
    // This is the result of ~12 hours of trying to get the UV override to play nicely
    // with both normal mapping and parallax mapping, and this is what worked in the end.
    vec3 bitangent = cross(inNormal, inTangent);
    mat3 tbn = mat3(inTangent, bitangent, inNormal);
    if (inUvDir == 1) {
        tbn = mat3(vec3(0.0, 0.0, 1.0) * -sign(inNormal.x) * -sign(inUV.x),
                vec3(0.0, 1.0, 0.0) * -sign(inNormal.x) * sign(inUV.y),
                vec3(1.0, 0.0, 0.0)) * sign(inNormal.x);
    } else if (inUvDir == 2) {
        tbn = mat3(vec3(1.0, 0.0, 0.0) * sign(inNormal.y) * sign(inUV.x),
                vec3(0.0, 0.0, 1.0) * sign(inNormal.y),
                vec3(0.0, 1.0, 0.0)) * sign(inNormal.y);
    } else if (inUvDir == 3) {
        tbn = mat3(vec3(1.0, 0.0, 0.0) * -sign(inNormal.z) * -sign(inUV.x),
                vec3(0.0, 1.0, 0.0) * -sign(inNormal.z) * sign(inUV.y),
                vec3(0.0, 0.0, 1.0)) * sign(inNormal.z);
    }

    vec2 tCoord = abs(inUV);


    float roughness = mat.roughness;
    float metallic = mat.metallic;
    float ao = 1.0;
    float surfaceDepth = 0.0;

    if (mat.heightmapIdx > -1 && DO_PARALLAX) {
        mat3 tbnT = transpose(tbn);
        vec3 tViewDir = normalize((tbnT * viewPos[gl_ViewIndex].xyz) - (tbnT * inWorldPos.xyz));
        surfaceDepth = 1.0 - texture(tex2dSampler[mat.heightmapIdx], tCoord).x;
        tCoord = parallaxMapping(tCoord, tViewDir, tex2dSampler[mat.heightmapIdx], mat.heightScale, PARALLAX_MIN_LAYERS, PARALLAX_MAX_LAYERS);
    }

    if ((flags & 0x1) == 0x1) {
        // Treat the rough texture as a packed PBR file
        // R = Metallic, G = Roughness, B = AO
        vec3 packVals = pow(texture(tex2dSampler[mat.roughTexIdx], tCoord).xyz, vec3(1.0 / 2.2));
        metallic = packVals.r;
        roughness = packVals.g;
        ao = packVals.b;
    } else {
        if (mat.roughTexIdx > -1)
            roughness = pow(texture(tex2dSampler[mat.roughTexIdx], tCoord).x, 1.0 / 2.2);

        if (mat.metalTexIdx > -1)
            metallic = pow(texture(tex2dSampler[mat.metalTexIdx], tCoord).x, 1.0 / 2.2);

        if (mat.aoTexIdx > -1)
            ao = pow(texture(tex2dSampler[mat.aoTexIdx], tCoord).x, 1.0 / 2.2);
    }

    uint doPicking = miscFlag & 0x1;

    vec4 albedoCol = texture(tex2dSampler[mat.albedoTexIdx], tCoord) * vec4(mat.albedoColor, 1.0);
#ifdef DEBUG
    if ((miscFlag & 64) == 64) {
        albedoCol.rgb = vec3(1.0);
    }
#endif
    vec3 f0 = mix(vec3(0.04), albedoCol.rgb, metallic);

    vec3 normal = normalize(mat.normalTexIdx > -1 ? getNormalMapNormal(mat, tCoord, tbn) : inNormal);

    ao *= calcProxyAO(inWorldPos.xyz, inNormal);

#ifdef DEBUG
    // debug views
    if ((miscFlag & 2) == 2) {
        // show normals
        FragColor = vec4((normal * 0.5) + 0.5, 1.0);
        return;
    } else if ((miscFlag & 4) == 4) {
        // show metallic
        FragColor = vec4(vec3(metallic), 1.0);
        return;
    } else if ((miscFlag & 8) == 8) {
        // show roughness
        FragColor = vec4(vec3(roughness), 1.0);
        return;
    } else if ((miscFlag & 16) == 16) {
        //show ao
        FragColor = vec4(vec3(ao), 1.0);
        return;
    } else if ((miscFlag & 32) == 32) {
        // show normal map value
        if (mat.normalTexIdx == ~0u) {
            FragColor = vec4(0.0, 0.0, 0.5, 1.0);
            return;
        }
        vec3 nMap = decodeNormal(texture(tex2dSampler[mat.normalTexIdx], tCoord).xy);
        FragColor = vec4(nMap, 1.0);
        return;
    } else if ((miscFlag & 2048) == 2048) {
        FragColor = vec4(mod(tCoord, vec2(1.0)), 0.0, 1.0);
        return;
    } else if ((miscFlag & 8192) == 8192) {
        vec4 whatevslol;
        int cascade = calculateCascade(whatevslol);
        FragColor = vec4((cascade == 0 ? 1.0f : 0.0f), (cascade == 1 ? 1.0f : 0.0f), (cascade == 2 ? 1.0f : 0.0f), 1.0f);
        return;
    }
#endif

    float finalAlpha = alphaCutoff > 0.0f ? albedoCol.a : 1.0f;
    if (alphaCutoff > 0.0f) {
        finalAlpha = (finalAlpha - alphaCutoff) / max(fwidth(finalAlpha), 0.0001) + 0.5;
    }

    ShadeInfo si;
    si.f0 = f0;
    si.metallic = metallic;
#if 0
    si.roughness = getAntiAliasedRoughness(roughness, normal);
#else
    si.roughness = roughness;
#endif
    si.albedoColor = albedoCol.rgb;
    si.normal = normal;
    si.alphaCutoff = alphaCutoff;
    si.viewDir = viewDir;
    si.ao = ao;

    FragColor = vec4(shade(si) + mat.emissiveColor, finalAlpha);

    if (ENABLE_PICKING && doPicking == 1) {
        handleEditorPicking();
    }
}
#endif
