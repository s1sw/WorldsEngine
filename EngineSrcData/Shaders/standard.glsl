#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_ARB_shader_ballot : require
#define MULTIVIEW
#ifdef MULTIVIEW
#extension GL_EXT_multiview : enable
#endif
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
layout(location = 2) VARYING(vec4, Tangent);
layout(location = 3) VARYING(vec2, UV);
layout(location = 4) VARYING(flat uint, UvDir);

#ifdef FRAGMENT
//#ifdef EFT
layout(early_fragment_tests) in;
//#endif
layout(location = 0) out vec4 FragColor;

layout(constant_id = 0) const bool ENABLE_PICKING = false;
layout(constant_id = 1) const float PARALLAX_MAX_LAYERS = 32.0;
layout(constant_id = 2) const float PARALLAX_MIN_LAYERS = 4.0;
layout(constant_id = 3) const bool DO_PARALLAX = false;
layout(constant_id = 4) const bool ENABLE_PROXY_AO = true;
#endif

#ifdef VERTEX
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in float inBitangentSign;
layout(location = 4) in vec2 inUV;
#ifdef SKINNED
layout(location = 5) in vec4 inBoneWeights;
layout(location = 6) in uvec4 inBoneIds;
#endif
#endif

#include <standard_descriptors.glsl>
#include <standard_push_constants.glsl>

vec3 getViewPos() {
#ifdef MULTIVIEW
    return viewPos[gl_ViewIndex].xyz;
#else
    return viewPos[0].xyz;
#endif
}

#ifdef VERTEX
void main() {
#ifdef SKINNED
    mat4 model = mat4(0.0f);

    model  = buf_BoneTransforms.matrices[skinningOffset + inBoneIds[0]] * inBoneWeights[0];
    model += buf_BoneTransforms.matrices[skinningOffset + inBoneIds[1]] * inBoneWeights[1];
    model += buf_BoneTransforms.matrices[skinningOffset + inBoneIds[2]] * inBoneWeights[2];
    model += buf_BoneTransforms.matrices[skinningOffset + inBoneIds[3]] * inBoneWeights[3];

    model = modelMatrices[modelMatrixIdx + gl_InstanceIndex] * model;
#else
    mat4 model = modelMatrices[modelMatrixIdx];
#endif

    int vpMatIdx = vpIdx;

#ifdef MULTIVIEW
    vpMatIdx += gl_ViewIndex;
#endif

    outWorldPos = (model * vec4(inPosition, 1.0));

    gl_Position = (projection[vpMatIdx] * view[vpMatIdx] * model) * vec4(inPosition, 1.0); // Apply MVP transform

    model = transpose(inverse(model));

    outNormal = normalize((model * vec4(inNormal, 0.0)).xyz);
    outTangent = normalize(vec4((model * vec4(inTangent, 0.0)).xyz, inBitangentSign));
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness

    vec2 uv = inUV;
    outUvDir = 0;

    if ((miscFlag & MISC_FLAG_UV_XY) == MISC_FLAG_UV_XY) {
        uv = outWorldPos.xy;
        outUvDir = 1;
    } else if ((miscFlag & MISC_FLAG_UV_XZ) == MISC_FLAG_UV_XZ) {
        uv = outWorldPos.xz;
        outUvDir = 2;
    } else if ((miscFlag & MISC_FLAG_UV_ZY) == MISC_FLAG_UV_ZY) {
        uv = outWorldPos.zy;
        outUvDir = 3;
    } else if ((miscFlag & MISC_FLAG_UV_PICK) == MISC_FLAG_UV_PICK) {
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
vec3 EnvBRDFApprox(vec3 F, float roughness, float NoV) {
    const vec4 c0 = { -1, -0.0275, -0.572, 0.022 };
    const vec4 c1 = { 1, 0.0425, 1.04, -0.04 };

    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2( -9.28 * NoV )) * r.x + r.y;
    vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;

    return F * AB.x + AB.y;
}

vec3 calcAmbient(vec3 f0, float roughness, vec3 viewDir, float metallic, vec3 albedoColor, vec3 normal) {
    const float MAX_REFLECTION_LOD = 6.0;
    vec3 R = reflect(-viewDir, normal);

    vec3 F = fresnelSchlickRoughness(clamp(dot(normal, viewDir), 0.0, 1.0), f0, roughness);

    if ((miscFlag & MISC_FLAG_CUBEMAP_PARALLAX) == MISC_FLAG_CUBEMAP_PARALLAX) {
        R = parallaxCorrect(R, cubemapPos, cubemapPos + cubemapExt, cubemapPos - cubemapExt, inWorldPos.xyz);
    }
    R.x = -R.x;
    R = normalize(R);

    vec3 specularAmbient = textureLod(cubemapSampler[cubemapIdx], R, roughness * MAX_REFLECTION_LOD).rgb * cubemapBoost;

#ifdef BRDF_APPROX
    vec3 specularColor = EnvBRDFApprox(F, roughness, max(dot(normal, viewDir), 0.0));
#else
    vec2 coord = vec2(roughness, max(dot(normal, viewDir), 0.0));
    vec2 brdf = textureLod(brdfLutSampler, coord, 0.0).rg;
    vec3 specularColor = F * (brdf.x + brdf.y);
#endif

    vec3 totalAmbient = specularAmbient * specularColor;

    if (metallic < 1.0) {
        vec3 kD = (1.0 - F) * (1.0 - metallic);
        totalAmbient += kD * textureLod(cubemapSampler[cubemapIdx], normal, 7.0).xyz * albedoColor * cubemapBoost;
    }

    return totalAmbient;
}

vec3 decodeNormal (vec2 texVal) {
    vec3 n;
    n.xy = texVal * 2.0 - 1.0;
    vec2 xySq = n.xy * n.xy;
    n.z = max(sqrt(1.0 - xySq.x - xySq.y), 0.0);
    return n;
}

vec3 getNormalMapNormal(Material mat, vec2 tCoord, mat3 tbn) {
    vec3 texNorm = decodeNormal(texture(tex2dSampler[mat.normalTexIdx], tCoord).xy);
    return normalize(tbn * texNorm);
}


void handleEditorPicking() {
    if (pixelPickCoords == ivec2(gl_FragCoord.xy)) {
        pickBuf.objectID = objectId;
    }
}

float mipMapLevel() {
#if 0
    return textureQueryLod(tex2dSampler[materials[matIdx].albedoTexIdx], inUV).x;
#else
    vec2 dx = dFdx(inUV);
    vec2 dy = dFdy(inUV);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));

    return max(0.0, 0.5 * log2(delta_max_sqr));
#endif
}

bool isTextureEnough(ivec2 texSize) {
    vec2  dx_vtc        = dFdx(inUV);
    vec2  dy_vtc        = dFdy(inUV);
    vec2 d = max(dx_vtc, dy_vtc);

    return all(greaterThan(d * vec2(texSize), vec2(4.0)));
}

float calculateCascade(out vec4 oShadowPos, out bool inCascade) {
    for (int i = 0; i < 4; i++) {
        vec4 shadowPos = dirShadowMatrices[i] * inWorldPos;
        shadowPos.y = -shadowPos.y;
        vec2 coord = (shadowPos.xy * 0.5 + 0.5);
        const float lThresh = (1.0 / cascadeTexelsPerUnit[i]);
        const float hThresh = 1.0 - lThresh;

        if (coord.x > lThresh && coord.x < hThresh &&
                coord.y > lThresh && coord.y < hThresh) {
            oShadowPos = shadowPos;
            inCascade = true;
            return float(i);
        }
    }
    inCascade = false;
    return -1.0;
}

float calcProxyAO(vec3 wPos, vec3 normal) {
    if (!ENABLE_PROXY_AO) return 1.0;
    float proxyAO = 1.0;

    //for (int i = 0; i < aoBoxCount; i++) {
    //    if (floatBitsToUint(aoBox[i].pack3.w) != objectId) {
    //        proxyAO *= (1.0 - getBoxOcclusionArtistic(aoBox[i], inWorldPos.xyz, normal));
    //        //proxyAO *= (1.0 - getBoxOcclusionNonClipped(aoBox[i], inWorldPos.xyz, normal));
    //    }
    //}
    //
    //for (int i = 0; i < aoSphereCount; i++) {
    //    if (sphereIds[i] != objectId) {
    //        proxyAO *= (1.0 - getSphereOcclusion(inWorldPos.xyz, normal, aoSphere[i]));
    //    }
    //}

    int tileIdxX = int(gl_FragCoord.x / buf_LightTileInfo.tileSize);
    int tileIdxY = int(gl_FragCoord.y / buf_LightTileInfo.tileSize);

    uint eyeOffset = buf_LightTileInfo.tilesPerEye * gl_ViewIndex;
    uint tileIdx = ((tileIdxY * buf_LightTileInfo.numTilesX) + tileIdxX) + eyeOffset;

    for (int i = 0; i < 2; i++) {
        //uint sphereBits = readFirstInvocationARB(subgroupOr(buf_LightTiles.tiles[tileIdx].aoSphereIdMasks[i]));
        uint sphereBits = buf_LightTiles.tiles[tileIdx].aoSphereIdMasks[i];

        while (sphereBits != 0) {
            // find the next set sphere bit
            uint sphereBitIndex = findLSB(sphereBits);

            // remove it from the mask with an XOR
            sphereBits ^= 1 << sphereBitIndex;

            uint realIndex = sphereBitIndex + (32 * i);

            if (sphereIds[realIndex] != objectId) {
                proxyAO *= (1.0 - getSphereOcclusion(inWorldPos.xyz, normal, aoSphere[realIndex]));
            }
        }
    }

    for (int i = 0; i < 2; i++) {
        uint boxBits = buf_LightTiles.tiles[tileIdx].aoBoxIdMasks[i];

        while (boxBits != 0) {
            // find the next set sphere bit
            uint boxBitIndex = findLSB(boxBits);

            // remove it from the mask with an XOR
            boxBits ^= 1 << boxBitIndex;

            uint realIndex = boxBitIndex + (32 * i);

            if (floatBitsToUint(aoBox[realIndex].pack3.w) != objectId) {
                //proxyAO *= (1.0 - getBoxOcclusionNonClipped(aoBox[realIndex], inWorldPos.xyz, normal));
                proxyAO *= (1.0 - getBoxOcclusionArtistic(aoBox[realIndex], inWorldPos.xyz, normal));
            }
        }
    }

    return proxyAO;
}

vec2 poissonDisk[64] = vec2[]( // don't use 'const' b/c of OSX GL compiler bug
    vec2(0.511749, 0.547686), vec2(0.58929, 0.257224), vec2(0.165018, 0.57663), vec2(0.407692, 0.742285),
    vec2(0.707012, 0.646523), vec2(0.31463, 0.466825), vec2(0.801257, 0.485186), vec2(0.418136, 0.146517),
    vec2(0.579889, 0.0368284), vec2(0.79801, 0.140114), vec2(-0.0413185, 0.371455), vec2(-0.0529108, 0.627352),
    vec2(0.0821375, 0.882071), vec2(0.17308, 0.301207), vec2(-0.120452, 0.867216), vec2(0.371096, 0.916454),
    vec2(-0.178381, 0.146101), vec2(-0.276489, 0.550525), vec2(0.12542, 0.126643), vec2(-0.296654, 0.286879),
    vec2(0.261744, -0.00604975), vec2(-0.213417, 0.715776), vec2(0.425684, -0.153211), vec2(-0.480054, 0.321357),
    vec2(-0.0717878, -0.0250567), vec2(-0.328775, -0.169666), vec2(-0.394923, 0.130802), vec2(-0.553681, -0.176777),
    vec2(-0.722615, 0.120616), vec2(-0.693065, 0.309017), vec2(0.603193, 0.791471), vec2(-0.0754941, -0.297988),
    vec2(0.109303, -0.156472), vec2(0.260605, -0.280111), vec2(0.129731, -0.487954), vec2(-0.537315, 0.520494),
    vec2(-0.42758, 0.800607), vec2(0.77309, -0.0728102), vec2(0.908777, 0.328356), vec2(0.985341, 0.0759158),
    vec2(0.947536, -0.11837), vec2(-0.103315, -0.610747), vec2(0.337171, -0.584), vec2(0.210919, -0.720055),
    vec2(0.41894, -0.36769), vec2(-0.254228, -0.49368), vec2(-0.428562, -0.404037), vec2(-0.831732, -0.189615),
    vec2(-0.922642, 0.0888026), vec2(-0.865914, 0.427795), vec2(0.706117, -0.311662), vec2(0.545465, -0.520942),
    vec2(-0.695738, 0.664492), vec2(0.389421, -0.899007), vec2(0.48842, -0.708054), vec2(0.760298, -0.62735),
    vec2(-0.390788, -0.707388), vec2(-0.591046, -0.686721), vec2(-0.769903, -0.413775), vec2(-0.604457, -0.502571),
    vec2(-0.557234, 0.00451362), vec2(0.147572, -0.924353), vec2(-0.0662488, -0.892081), vec2(0.863832, -0.407206)
);

float harden(float x) {
    x = 2.0 * x - 1.0;
    float xSign = sign(x);
    x = 1.0 - (xSign * x);
    x = x * x * x;
    x = 1.0 - x;
    x *= xSign;
    x = 0.5 * x + 0.5;

    return x;
}

float getDirLightShadowIntensity(int lightIdx) {
    vec4 shadowPos;
    float shadowIntensity = 1.0;
    bool inCascade = true;
    float cascadeSplit = calculateCascade(shadowPos, inCascade);

    float bias = max(0.0004 * (1.0 - dot(inNormal, lights[lightIdx].pack1.xyz)), 0.00025);
    //float bias = 0.000325;
    float depth = (shadowPos.z / shadowPos.w);
    vec2 coord = (shadowPos.xy * 0.5 + 0.5);

    if (!inCascade)
        return 1.0;

    //if (coord.x > 0.0 && coord.x < 1.0 &&
    //        coord.y > 0.0 && coord.y < 1.0 &&
    //        depth < 1.0 && depth > 0.0)
    {
#ifdef HIGH_QUALITY_SHADOWS
        float percentOccluded = 0.0;
        float distanceSum = 0.0;
        float kernelScale = (2.0 / 512.0); //* (textureSize(shadowSampler, 0).x / 1024.0);
        //float kernelScale = (1.0 / 512.0);

        const int NUM_TAPS = 32;

        for (int t = 0; t < NUM_TAPS; t++) {
            float smDepth = textureLod(shadowSampler, vec3(coord + (poissonDisk[t] * kernelScale), cascadeSplit), 0.0).x;
            float dist = (smDepth - depth);
            float occluded = step(bias, dist);

            percentOccluded += occluded;
            distanceSum += dist;
        }

        if (percentOccluded != 0.0) {
            float averageDistance = distanceSum / percentOccluded;
            percentOccluded /= NUM_TAPS;

            float pcfWeight = saturate(averageDistance * depth);
            percentOccluded = mix(harden(percentOccluded), percentOccluded, pcfWeight);
        }

        shadowIntensity = 1.0 - percentOccluded;
#else
        shadowIntensity = step(textureLod(shadowSampler, vec3(coord, float(cascadeSplit)), 0.0).x - depth, bias);
#endif
    }
    return shadowIntensity;
}


float pcf(vec3 sampleCoord, sampler2D samp, float bias) {
    float shadowIntensity = 0.0;
#ifdef HIGH_QUALITY_SHADOWS
    float percentOccluded = 0.0;
    float distanceSum = 0.0;
    float kernelScale = (1.5 / 512.0);

    const int NUM_TAPS = 16;

    for (int t = 0; t < NUM_TAPS; t++) {
        //vec4 smDepths = textureGather(samp, sampleCoord.xy + (poissonDisk[t] * kernelScale));
        float smDepth = textureLod(samp, sampleCoord.xy + (poissonDisk[t] * kernelScale), 0.0).x;
        float dist = (smDepth - sampleCoord.z);
        float occluded = step(bias, dist);

        percentOccluded += occluded;
        distanceSum += dist;
    }

    if (percentOccluded != 0.0) {
        float averageDistance = distanceSum / percentOccluded;
        percentOccluded /= NUM_TAPS;

        float pcfWeight = saturate(averageDistance / sampleCoord.z);
        percentOccluded = mix(harden(percentOccluded), percentOccluded, pcfWeight);
    }

    shadowIntensity = 1.0 - percentOccluded;
#else
    shadowIntensity = step(textureLod(samp, sampleCoord.xy, 0.0).x - sampleCoord.z, bias);
#endif
    return shadowIntensity;
}

float getNormalLightShadowIntensity(int lightIdx) {
    uint shadowIdx = getShadowmapIndex(lights[lightIdx]);
    vec4 shadowPos = otherShadowMatrices[shadowIdx] * inWorldPos;
    shadowPos.y = -shadowPos.y;

    float bias = max(0.0005 * (1.0 - dot(inNormal, getLightDirection(lights[lightIdx], inWorldPos.xyz))), 0.00004);

    float depth = (shadowPos.z / shadowPos.w);
    vec2 coord = (shadowPos.xy / shadowPos.w) * 0.5 + 0.5;//(shadowPos.xy * 0.5 + 0.5);

    float shadowIntensity = 1.0;

    if (coord.x > 0.0 && coord.x < 1.0 &&
            coord.y > 0.0 && coord.y < 1.0 &&
            depth < 1.0 && depth > 0.0) {
        shadowIntensity = pcf(vec3(coord, depth), additionalShadowSampler[shadowIdx], bias);
    }
    return shadowIntensity;
}

vec3 shadeLight(int lightIndex, ShadeInfo si) {
    vec3 l = calculateLighting(lights[lightIndex], si, inWorldPos.xyz);

    float shadowIntensity = 1.0;

    if (getLightType(lights[lightIndex]) == LT_DIRECTIONAL && !((miscFlag & MISC_FLAG_DISABLE_SHADOWS) == MISC_FLAG_DISABLE_SHADOWS)) {
        shadowIntensity = getDirLightShadowIntensity(lightIndex);
    } else if (getShadowmapIndex(lights[lightIndex]) != 0xF) {
        shadowIntensity = getNormalLightShadowIntensity(lightIndex);
    }

    return l * shadowIntensity;
}

vec3 shade(ShadeInfo si) {
    int tileIdxX = int(gl_FragCoord.x / buf_LightTileInfo.tileSize);
    int tileIdxY = int(gl_FragCoord.y / buf_LightTileInfo.tileSize);

    uint eyeOffset = buf_LightTileInfo.tilesPerEye * gl_ViewIndex;
    uint tileIdx = ((tileIdxY * buf_LightTileInfo.numTilesX) + tileIdxX) + eyeOffset;

    vec3 f0 = mix(vec3(0.04), si.albedoColor, si.metallic);
    vec3 ambient = calcAmbient(f0, si.roughness, si.viewDir, si.metallic, si.albedoColor, si.normal);
    //return ambient * si.ao;

    vec3 lo = vec3(0.0);
#define TILED
#ifdef TILED
    for (int i = 0; i < 8; i++) {
        uint lightBits = readFirstInvocationARB(subgroupOr(buf_LightTiles.tiles[tileIdx].lightIdMasks[i]));

        while (lightBits != 0) {
            // find the next set light bit
            uint lightBitIndex = findLSB(lightBits);

            // remove it from the mask with an XOR
            lightBits ^= 1 << lightBitIndex;

            uint realIndex = lightBitIndex + (32 * i);
            lo += shadeLight(int(realIndex), si);
        }
    }
#else
    for (int i = 0; i < lightCount; i++) {
        lo += shadeLight(i, si);
    }
#endif

    return (ambient * si.ao) + lo;
}

float getAntiAliasedRoughness(float inRoughness, vec3 normal) {
    vec3 ddxN = dFdx(normal);
    vec3 ddyN = dFdy(normal);

    float geoRoughness = pow(saturate(max(dot(ddxN.xyz, ddxN.xyz), dot(ddyN.xyz, ddyN.xyz))), 0.333);
    return max(inRoughness, geoRoughness);
}

mat3 cotangent_frame( vec3 N, vec3 p, vec2 uv ) {
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx( p );
    vec3 dp2 = dFdy( p );
    vec2 duv1 = dFdx( uv );
    vec2 duv2 = dFdy( uv );
    // solve the linear system
    vec3 dp2perp = cross( dp2, N );
    vec3 dp1perp = cross( N, dp1 );
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    // construct a scale-invariant frame
    float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
    return mat3( T * invmax, B * invmax, N );
}

#define MATERIAL_FLAG_PACKED_PBR (1 << 8)
void unpackMaterial(inout ShadeInfo si, vec2 tCoord, mat3 tbn) {
    Material mat = materials[matIdx];
    si.metallic = mat.metallic;
    si.roughness = mat.roughness;
    //#ifndef EFT
    //    float alphaCutoff = (mat.cutoffFlags & (0xFF)) / 255.0f;
    //    si.alphaCutoff = alphaCutoff;
    //#else
    si.alphaCutoff = 0.0;
    //#endif

    if (mat.heightmapIdx > -1 && DO_PARALLAX) {
        mat3 tbnT = (tbn);
        vec3 tViewDir = normalize((tbnT * getViewPos()) - (tbnT * inWorldPos.xyz));
        tCoord = parallaxMapping(tCoord, tViewDir, tex2dSampler[mat.heightmapIdx], mat.heightScale, PARALLAX_MIN_LAYERS, PARALLAX_MAX_LAYERS);
    }
    si.ao = 1.0;

    if ((mat.cutoffFlags & MATERIAL_FLAG_PACKED_PBR) == MATERIAL_FLAG_PACKED_PBR) {
        // Treat the rough texture as a packed PBR file
        // R = Metallic, G = Roughness, B = AO
        vec3 packVals = texture(tex2dSampler[mat.roughTexIdx], tCoord).xyz;
        si.metallic = packVals.r;
        si.roughness = packVals.g;
        si.ao = packVals.b;
    }
    else {
        if (mat.roughTexIdx > -1)
            si.roughness = texture(tex2dSampler[mat.roughTexIdx], tCoord).x;

        if (mat.metalTexIdx > -1)
            si.metallic = texture(tex2dSampler[mat.metalTexIdx], tCoord).x;

        if (mat.aoTexIdx > -1)
            si.ao = texture(tex2dSampler[mat.aoTexIdx], tCoord).x;
    }

    vec4 albedoColor = texture(tex2dSampler[mat.albedoTexIdx], tCoord) * vec4(mat.albedoColor, 1.0);
#ifdef DEBUG
    if ((miscFlag & DBG_FLAG_LIGHTING_ONLY) == DBG_FLAG_LIGHTING_ONLY) {
        albedoColor.rgb = vec3(1.0);
    }
#endif
    si.albedoColor = albedoColor.rgb;
    si.normal = mat.normalTexIdx > -1 ? getNormalMapNormal(mat, tCoord, tbn) : inNormal;
    si.ao *= calcProxyAO(inWorldPos.xyz, si.normal);
    //si.roughness = getAntiAliasedRoughness(si.roughness, si.normal);
    si.alpha = albedoColor.a;
    si.emissive = mat.emissiveColor;
}

void main() {
#ifdef MIP_MAP_DBG
    float mipLevel = min(3, mipMapLevel());

    FragColor = vec4(vec3(mipLevel < 0.1, mipLevel * 0.5, mipLevel * 0.25), 0.0);
    FragColor = max(FragColor, vec4(0.0));
    return;
#endif

#ifdef TEXRES_DBG
    bool ite = isTextureEnough(textureSize(tex2dSampler[materials[matIdx].albedoTexIdx], 0));
    if (!ite) {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }
#endif

    // I'm going to be honest: I have no clue what the fuck is happening here.
    // This is the result of ~12 hours of trying to get the UV override to play nicely
    // with both normal mapping and parallax mapping, and this is what worked in the end.
    vec3 bitangent = cross(inNormal, inTangent.xyz) * inTangent.w;
    mat3 tbn = mat3(inTangent.xyz, bitangent, inNormal);
    //if (inUvDir == 1) {
    //    tbn = mat3(vec3(0.0, 0.0, 1.0) * -sign(inNormal.x) * -sign(inUV.x),
    //            vec3(0.0, 1.0, 0.0) * -sign(inNormal.x) * sign(inUV.y),
    //            vec3(1.0, 0.0, 0.0)) * sign(inNormal.x);
    //} else if (inUvDir == 2) {
    //    tbn = mat3(vec3(1.0, 0.0, 0.0)  * sign(inNormal.y) * sign(inUV.x),
    //               vec3(0.0, 0.0, 1.0)  * sign(inNormal.y),
    //               vec3(0.0, 1.0, 0.0)) * sign(inNormal.y);
    //} else if (inUvDir == 3) {
    //    tbn = mat3(vec3(1.0, 0.0, 0.0) * -sign(inNormal.z) * -sign(inUV.x),
    //            vec3(0.0, 1.0, 0.0) * -sign(inNormal.z) * sign(inUV.y),
    //            vec3(0.0, 0.0, 1.0)) * sign(inNormal.z);
    //}
    // Let's attempt some tangent frame construction on the fly!
    vec2 tCoord = inUV;
    if (inUvDir != 0) {
        tbn = cotangent_frame(inNormal, getViewPos() - inWorldPos.xyz, tCoord);
        //if (inUV.x < 0.0) {
        //    tCoord.x += 20.0; 
        //}
        //
        //if (inUV.y < 0.0) {
        //    tCoord.y += 20.0;//= mod(tCoord.y, 1.0) + 1.0;
        //}
    }

    uint doPicking = miscFlag & 0x1;

    if (ENABLE_PICKING && doPicking == 1) {
        handleEditorPicking();
    }

    ShadeInfo si;
    unpackMaterial(si, tCoord, tbn);
    si.viewDir = normalize(getViewPos() - inWorldPos.xyz);

#ifdef DEBUG
    // debug views
    if ((miscFlag & DBG_FLAG_NORMALS) == DBG_FLAG_NORMALS) {
        // show normals
        FragColor = vec4((si.normal * 0.5) + 0.5, 1.0);
        return;
    } else if ((miscFlag & DBG_FLAG_METALLIC) == DBG_FLAG_METALLIC) {
        // show metallic
        FragColor = vec4(vec3(si.metallic), 1.0);
        return;
    } else if ((miscFlag & DBG_FLAG_ROUGHNESS) == DBG_FLAG_ROUGHNESS) {
        // show roughness
        FragColor = vec4(vec3(si.roughness), 1.0);
        return;
    } else if ((miscFlag & DBG_FLAG_AO) == DBG_FLAG_AO) {
        //show ao
        FragColor = vec4(vec3(si.ao), 1.0);
        return;
    } else if ((miscFlag & DBG_FLAG_NORMAL_MAP) == DBG_FLAG_NORMAL_MAP) {
        // show normal map value
        if (materials[matIdx].normalTexIdx == ~0u) {
            FragColor = vec4(0.0, 0.0, 0.5, 1.0);
            return;
        }
        vec3 nMap = decodeNormal(texture(tex2dSampler[materials[matIdx].normalTexIdx], inUV).xy);
        FragColor = vec4(pow((nMap * 0.5) + 0.5, vec3(2.2)), 1.0);
        return;
    } else if ((miscFlag & DBG_FLAG_UVS) == DBG_FLAG_UVS) {
        FragColor = vec4(mod(inUV, vec2(1.0)), 0.0, 1.0);
        return;
    } else if ((miscFlag & DBG_FLAG_SHADOW_CASCADES) == DBG_FLAG_SHADOW_CASCADES) {
        vec4 whatevslol;
        bool whatevs2;
        float cascade = calculateCascade(whatevslol, whatevs2);
        FragColor = vec4((cascade == 0 ? 1.0f : 0.0f), (cascade == 1 ? 1.0f : 0.0f), (cascade == 2 ? 1.0f : 0.0f), 1.0f);
        return;
    } else if ((miscFlag & DBG_FLAG_ALBEDO) == DBG_FLAG_ALBEDO) {
        FragColor = vec4(si.albedoColor, 1.0);
        return;
    } else if ((miscFlag & DBG_FLAG_LIGHT_TILES) == DBG_FLAG_LIGHT_TILES) {
        int tileIdxX = int(gl_FragCoord.x / buf_LightTileInfo.tileSize);
        int tileIdxY = int(gl_FragCoord.y / buf_LightTileInfo.tileSize);

        uint tileIdx = ((tileIdxY * buf_LightTileInfo.numTilesX) + tileIdxX) + (buf_LightTileInfo.tilesPerEye * gl_ViewIndex);
        int lightCount = int(buf_LightTileLightCounts.tileLightCounts[tileIdx]);

        //vec3 heatmapCol = mix(vec3(0.0), vec3(1.0), lightCount / 64.0);
        vec3 heatmapCol = vec3(saturate((lightCount - 8.0) / 4.0), saturate(lightCount / 4.0), saturate((lightCount - 4.0) / 4.0));

        if (int(gl_FragCoord.x) % int(buf_LightTileInfo.tileSize) == 0 || int(gl_FragCoord.y) % int(buf_LightTileInfo.tileSize) == 0)
            heatmapCol.z = 1.0;

        FragColor = vec4(mix(shade(si), heatmapCol, 0.25), 1.0);
        return;
    }
#endif

    //#ifndef EFT
    //    float finalAlpha = si.alphaCutoff > 0.0f ? si.alpha : 1.0f;
    //    if (si.alphaCutoff > 0.0f) {
    //        //finalAlpha *= 1 + mipMapLevel() * 0.75;
    //        finalAlpha = (finalAlpha - si.alphaCutoff) / max(fwidth(finalAlpha), 0.0001) + 0.5;
    //    }
    //#else
    //    float finalAlpha = 1.0f;
    //#endif

    FragColor = vec4(shade(si) + si.emissive, 1.0);
}
#endif
