#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_multiview : enable
#include <light.glsl>
#include <material.glsl>
#include <pbrutil.glsl>
#include <pbrshade.glsl>
#include <parallax.glsl>

#ifdef FRAGMENT
#ifdef EFT
layout(early_fragment_tests) in;
#endif
layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec4 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inShadowPos;
layout(location = 5) in float inDepth;
layout(location = 6) in flat uint inUvDir;

layout(constant_id = 0) const bool ENABLE_PICKING = false;
layout(constant_id = 1) const bool FACE_PICKING = false;
#endif

#ifdef VERTEX
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec4 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec2 outUV;
layout(location = 4) out vec4 outShadowPos;
layout(location = 5) out float outDepth;
layout(location = 6) out flat uint outUvDir;
#endif

layout(binding = 0) uniform MultiVP {
    mat4 view[4];
    mat4 projection[4];
    vec4 viewPos[4];
};

layout(std140, binding = 1) uniform LightBuffer {
    // (light count, yzw unused)
    vec4 pack0;
    mat4 shadowmapMatrix;
    Light lights[128];
};

layout(std140, binding = 2) uniform MaterialSettingsBuffer {
    Material materials[256];
};

layout(std140, binding = 3) uniform ModelMatrices {
    mat4 modelMatrices[1024];
};

layout (binding = 4) uniform sampler2D tex2dSampler[];
layout (binding = 5) uniform sampler2DShadow shadowSampler;
layout (binding = 6) uniform samplerCube cubemapSampler[];
layout (binding = 7) uniform sampler2D brdfLutSampler;

layout(std430, binding = 8) buffer PickingBuffer {
    uint objectID;
} pickBuf;

layout(push_constant) uniform PushConstants {
    vec4 texScaleOffset;

    int modelMatrixIdx;
    int matIdx;
    int vpIdx;
    uint objectId;

    ivec2 pixelPickCoords;
    // Misc flag uint
    // 32 bits
    // 1 - Activate object picking             (1)
    // 2 - Debug display normals               (2)
    // 3 - Debug display metallic              (4)
    // 4 - Debug display roughness             (8)
    // 5 - Debug display AO                    (16)
    // 6 - Debug display normal map            (32)
    // 7 - Lighting only                       (64)
    // 8 - World space UVs (XY)                (128)
    // 9 - World space UVs (XZ)                (256)
    // 10 - World space UVs (ZY)               (512)
    // 11 - World space UVs (pick with normal) (1024)
    // 12 - Debug display UVs                  (2048)
    uint miscFlag;
    uint cubemapIdx;
};

#ifdef VERTEX
void main() {
    mat4 model = modelMatrices[modelMatrixIdx + gl_InstanceIndex];

    int vpMatIdx = vpIdx + gl_ViewIndex;
    outWorldPos = (model * vec4(inPosition, 1.0));

    mat4 projMat = projection[vpMatIdx];

    gl_Position = projection[vpMatIdx] * view[vpMatIdx] * outWorldPos; // Apply MVP transform

    mat3 model3 = mat3(model);
    // remove scaling
    model3[0] = normalize(model3[0]);
    model3[1] = normalize(model3[1]);
    model3[2] = normalize(model3[2]);
    outNormal = normalize(model3 * inNormal);
    outTangent = normalize(model3 * inTangent);
    outShadowPos = shadowmapMatrix * outWorldPos;
    outShadowPos.y = -outShadowPos.y;
    outDepth = gl_Position.z / gl_Position.w;
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
            //outNormal = vec3(-1.0, 0.0, 0.0);
            outTangent = vec3(0.0, 1.0, 0.0);
        } else if (dots.y == maxProduct) {
            uv = outWorldPos.xz;
            outUvDir = 2;
            outTangent = vec3(1.0, 0.0, 0.0);
        } else {
            uv = outWorldPos.xy;
            outUvDir = 3;
            outTangent = vec3(1.0, 0.0, 0.0);
        }
        //outTangent = model3 * outTangent;
    }

    outUV = (uv * texScaleOffset.xy) + texScaleOffset.zw;
}
#endif

#ifdef FRAGMENT
vec3 calcAmbient(vec3 f0, float roughness, vec3 viewDir, float metallic, vec3 albedoColor, vec3 normal) {
    vec3 F = fresnelSchlickRoughness(clamp(dot(normal, viewDir), 0.0, 1.0), f0, roughness);

    const float MAX_REFLECTION_LOD = 7.0;
    vec3 R = reflect(-viewDir, normal);

    vec3 specularAmbient = textureLod(cubemapSampler[cubemapIdx], R, roughness * MAX_REFLECTION_LOD).rgb;

    vec2 brdf  = textureLod(brdfLutSampler, vec2(max(dot(normal, viewDir), 0.0), roughness), 0.0).rg;

    vec3 specularColor = (F * (brdf.x + brdf.y));

    float horizon = min(1.0 + dot(R, normal), 1.0);
    specularAmbient *= horizon * horizon;
    vec3 diffuseAmbient = textureLod(cubemapSampler[cubemapIdx], normal, 7.0).xyz * albedoColor;

    vec3 kD = (1.0 - F) * (1.0 - metallic);

    return kD * diffuseAmbient + (specularAmbient * specularColor);
}

vec3 decodeNormal (vec2 texVal) {
    vec3 n;
    n.xy = (texVal*2.0)-1.0;
    n.z = sqrt(1.0 - (n.x * n.x) - (n.y * n.y));
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

float mip_map_level() {
    return textureQueryLod(tex2dSampler[materials[matIdx].albedoTexIdx], inUV).x;
}

bool isTextureEnough(ivec2 texSize) {
    vec2  dx_vtc        = dFdx(inUV);
    vec2  dy_vtc        = dFdy(inUV);
    vec2 d = max(dx_vtc, dy_vtc);

    return all(greaterThan(d * vec2(texSize), vec2(4.0)));
}

vec3 shade(ShadeInfo si) {
    int lightCount = int(pack0.x);

    vec3 lo = vec3(0.0);
    for (int i = 0; i < lightCount; i++) {
        float shadowIntensity = 1.0;
        if (int(lights[i].pack0.w) == LT_DIRECTIONAL) {
            float bias = max(0.00005 * (1.0 - dot(inNormal, lights[i].pack1.xyz)), 0.00001);
            float depth = (inShadowPos.z / inShadowPos.w) - bias;
            vec2 coord = (inShadowPos.xy * 0.5 + 0.5);

            if (coord.x > 0.0 && coord.x < 1.0 && coord.y > 0.0 && coord.y < 1.0 && depth < 1.0 && depth > 0.0) {
                float texelSize = 1.0 / textureSize(shadowSampler, 0).x;
                shadowIntensity = 0.0;

                const int shadowSamples = 2;
                const float divVal = ((shadowSamples * 2)) * ((shadowSamples * 2));

                for (int x = -shadowSamples; x < shadowSamples; x++)
                    for (int y = -shadowSamples; y < shadowSamples; y++)
                        shadowIntensity += texture(shadowSampler, vec3(coord + vec2(x, y) * texelSize, depth)).x;

                shadowIntensity /= divVal;
            }
        }
        lo += shadowIntensity * calculateLighting(lights[i], si, inWorldPos.xyz);
    }

    vec3 ambient = calcAmbient(si.f0, si.roughness, si.viewDir, si.metallic, si.albedoColor, si.normal);
    return lo + (ambient * si.ao);
}

void main() {
    Material mat = materials[matIdx];

    float alphaCutoff = (mat.cutoffFlags & (0xFF)) / 255.0f;
    uint flags = (mat.cutoffFlags & (0x7FFFFF80)) >> 8;

#ifdef MIP_MAP_DBG
    float mipLevel = min(3, mip_map_level());

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
    mat3 tbnT = transpose(tbn);

    vec2 tCoord = abs(inUV);

    vec3 tViewDir = normalize((tbnT * viewPos[gl_ViewIndex].xyz) - (tbnT * inWorldPos.xyz));

    float roughness = mat.roughness;
    float metallic = mat.metallic;
    float ao = 1.0;

    if (mat.heightmapIdx > -1)
        tCoord = parallaxMapping(tCoord, tViewDir, tex2dSampler[mat.heightmapIdx], mat.heightScale);

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
    vec3 f0 = mix(vec3(0.04), albedoCol.rgb, metallic);

    vec3 normal = mat.normalTexIdx > -1 ? getNormalMapNormal(mat, tCoord, tbn) : inNormal;

    // debug views
    if ((miscFlag & 2) == 2) {
        // show normals
        //FragColor = vec4((normal * 0.5) + 0.5, 1.0);
        FragColor = vec4(vec3(dot(normal, vec3(0.0, 1.0, 0.0))), 1.0);
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
    }

    if ((miscFlag & 64) == 64) {
        albedoCol.rgb = vec3(1.0);
    }

    float finalAlpha = alphaCutoff > 0.0f ? albedoCol.a : 1.0f;

    if (alphaCutoff > 0.0f) {
        finalAlpha = (finalAlpha - alphaCutoff) / max(fwidth(finalAlpha), 0.0001) + 0.5;
    }

    if (finalAlpha == 0.0) discard;

    ShadeInfo si;
    si.f0 = f0;
    si.metallic = metallic;
    si.roughness = roughness;
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
