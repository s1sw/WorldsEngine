#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_multiview : enable
#define PI 3.1415926535

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
#ifdef LIGHTMAP
layout(location = 6) in vec2 inLightmapUV;
#endif

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
#ifdef LIGHTMAP
layout(location = 6) in vec2 inLightmapUV;
#endif
#endif

const int LT_POINT = 0;
const int LT_SPOT = 1;
const int LT_DIRECTIONAL = 2;

layout(binding = 0) uniform MultiVP {
	mat4 view[4];
	mat4 projection[4];
    vec4 viewPos[4];
};

struct Light {
	// (color rgb, type)
	vec4 pack0;
	// (direction xyz, spotlight cutoff)
	vec4 pack1;
	// (position xyz, unused)
	vec4 pack2;
};

layout(std140, binding = 1) uniform LightBuffer {
	// (light count, yzw unused)
	vec4 pack0;
	mat4 shadowmapMatrix;
	Light lights[128];
};

struct Material {
    float metallic;
    float roughness;
    int albedoTexIdx;
    int normalTexIdx;

    vec3 albedoColor;
    float alphaCutoff;

    int heightmapIdx;
    float heightScale;
    float pad0;
    float pad1;

    vec3 emissiveColor;
    float pad2;
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
    uint doPicking;
    uint cubemapIdx;
};

#ifdef VERTEX
void main() {
	mat4 model = modelMatrices[modelMatrixIdx];

    // On AMD driver 20.10.1 (and possibly earlier) using gl_ViewIndex seems to cause a driver crash
    int vpMatIdx = vpIdx; // + gl_ViewIndex; 

    #ifndef AMD_VIEWINDEX_WORKAROUND
    vpMatIdx += gl_ViewIndex;
    #endif
    outWorldPos = (model * vec4(inPosition, 1.0));

    mat4 projMat = projection[vpMatIdx];

    gl_Position = projection[vpMatIdx] * view[vpMatIdx] * outWorldPos; // Apply MVP transform
	
    outUV = (inUV * texScaleOffset.xy) + texScaleOffset.zw;
    outNormal = normalize(model * vec4(inNormal, 0.0)).xyz;
    outTangent = normalize(model * vec4(inTangent, 0.0)).xyz;
	outShadowPos = shadowmapMatrix * outWorldPos;
    outShadowPos.y = -outShadowPos.y;
	outDepth = gl_Position.z / gl_Position.w;
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
}
#endif

#ifdef FRAGMENT
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
    float alphaSq = (roughness * roughness) * (roughness * roughness);

    float denom = max((cosLh * cosLh) * (alphaSq - 1.0) + 1.0, 0.0001);
    return alphaSq / (PI * denom * denom);
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

vec3 calculateLighting(int lightIdx, vec3 viewDir, vec3 f0, float metallic, float roughness, vec3 albedoColor, vec3 normal) {
	Light light = lights[lightIdx];
	int lightType = int(light.pack0.w);
    vec3 radiance = light.pack0.xyz;
    vec3 L = vec3(0.0f, 0.0f, 0.0f);

    if (dot(normal, viewDir) < 0.0)
        normal = -normal;
	
    if (lightType == LT_POINT) {
    	vec3 lightPos = light.pack2.xyz;
        L = lightPos - inWorldPos.xyz;
        // dot(L, L) = length(L) squared
        radiance *= 1.0 / dot(L, L);
		L = normalize(L);
    } else if (lightType == LT_SPOT) {
	    vec3 lightPos = light.pack2.xyz;
        L = normalize(lightPos - inWorldPos.xyz);
        float theta = dot(L, normalize(light.pack1.xyz));
        float cutoff = light.pack1.w;
        float outerCutoff = cutoff - 0.02f;
        vec3 lToFrag = lightPos - inWorldPos.xyz;
        radiance *= clamp((theta - outerCutoff) / (cutoff - outerCutoff), 0.0f, 1.0f) * (1.0 / dot(lToFrag, lToFrag));
    } else {
        L = normalize(light.pack1.xyz);
    }

    // Boost the roughness a little bit for analytical lights otherwise the reflection of the light turns invisible
    roughness = clamp(roughness + 0.05f, 0.001, 1.0);

    vec3 halfway = normalize(viewDir + L);
    vec3 norm = normalize(normal);
    float cosLh = max(0.0f, dot(norm, halfway));
    float cosLi = max(0.0f, dot(norm, L));

    #ifdef BLINN_PHONG

    float specIntensity = pow(cosLh, (1.0 / max(roughness, 0.001)) * 50.0) * (1.0 - (roughness * roughness));

    return (vec3(specIntensity) * radiance) + (1.0 - metallic) * (albedoColor * radiance * cosLi); 

    #else
    float cosLo = max(0.0f, dot(norm, viewDir));

    float NDF = ndfGGX(cosLh, roughness);
    float G = gaSchlickGGX(cosLi, cosLo, roughness);
    vec3 f = fresnelSchlick(f0, max(dot(halfway, viewDir), 0.0f));

    vec3 kd = mix(vec3(1.0) - f, vec3(0.0), metallic);

    vec3 numerator = NDF * G * f;
    float denominator = 4.0f * cosLo * cosLi;
    vec3 specular = numerator / max(denominator, 0.001f);

    return (specular + (kd * albedoColor)) * radiance * cosLi;
    #endif
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 decodeNormal (vec2 texVal) {
    vec3 n;
    n.xy = texVal*2-1;
    n.z = sqrt(1-dot(n.xy, n.xy));
    return n;
}

vec3 calcAmbient(vec3 f0, float roughness, vec3 viewDir, float metallic, vec3 albedoColor, vec3 normal) {
    vec3 kD = (1.0 - fresnelSchlickRoughness(clamp(dot(normal, viewDir), 0.0, 1.0), f0, roughness)) * (1.0 - metallic);

    const float MAX_REFLECTION_LOD = 11.0;
    vec3 R = reflect(-viewDir, normal);

    vec3 specularAmbient = textureLod(cubemapSampler[cubemapIdx], R, roughness * MAX_REFLECTION_LOD).rgb;

    vec2 brdf  = textureLod(brdfLutSampler, vec2(min(max(dot(normal, viewDir), 0.0), 0.95), roughness), 0.0).rg;
    float f90 = clamp(50.0 * f0.g, 0.0, 1.0);

    vec3 specularColor = (f0 * brdf.x) + (brdf.y * f90);

    float horizon = min(1.0 + dot(R, normal), 1.0);
    specularAmbient *= horizon * horizon;
    vec3 diffuseAmbient = textureLod(cubemapSampler[cubemapIdx], normal, MAX_REFLECTION_LOD * 0.5).xyz * 0.5 * albedoColor;

    return kD * diffuseAmbient + (specularAmbient * specularColor);
}

vec3 getNormalMapNormal(Material mat, vec2 tCoord, mat3 tbn) {
    vec3 texNorm = normalize(decodeNormal(texture(tex2dSampler[mat.normalTexIdx], tCoord).xy));
    return normalize(tbn * texNorm);
}

vec2 pm(vec2 texCoords, vec3 viewDir, Material mat) { 
    float height = texture(tex2dSampler[mat.heightmapIdx], texCoords).r;    
    vec2 p = viewDir.xy / viewDir.z * (height * mat.heightScale);
    return texCoords - p;    
}

vec2 pom(vec2 texCoords, vec3 viewDir, Material mat) { 
    // number of depth layers
    const float minLayers = 8;
    const float maxLayers = 32;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));  
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy / viewDir.z * mat.heightScale; 
    vec2 deltaTexCoords = P / numLayers;
  
    // get initial values
    vec2  currentTexCoords     = texCoords;
    float currentDepthMapValue = texture(tex2dSampler[mat.heightmapIdx], currentTexCoords).r;
    
    const int maxIter = 10;
    int iter = 0;
    while(currentLayerDepth < currentDepthMapValue)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = texture(tex2dSampler[mat.heightmapIdx], currentTexCoords).r;  
        // get depth of next layer
        currentLayerDepth += layerDepth;
        iter++;

        if (iter >= maxIter)
            break;
    }
    
    // get texture coordinates before collision (reverse operations)
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(tex2dSampler[mat.heightmapIdx], prevTexCoords).r - currentLayerDepth + layerDepth;
 
    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

void handleEditorPicking() {
    if (pixelPickCoords == ivec2(gl_FragCoord.xy)) {
        pickBuf.objectID = objectId;
    }
}

float mip_map_level() {
    vec2  dx_vtc        = dFdx(inUV);
    vec2  dy_vtc        = dFdy(inUV);
    float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));

    return 0.5 * log2(delta_max_sqr); // == log2(sqrt(delta_max_sqr));
}

bool isTextureEnough(ivec2 texSize) {
    vec2  dx_vtc        = dFdx(inUV);
    vec2  dy_vtc        = dFdy(inUV);
    vec2 d = max(dx_vtc, dy_vtc);

    return all(greaterThan(d * vec2(texSize), vec2(4.0)));
}

void main() {
    //pickBuf.objectID = 0;
    Material mat = materials[matIdx];


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

#ifndef AMD_VIEWINDEX_WORKAROUND
	vec3 viewDir = normalize(viewPos[gl_ViewIndex].xyz - inWorldPos.xyz);
#else
	vec3 viewDir = normalize(viewPos[0].xyz - inWorldPos.xyz);
#endif

	int lightCount = int(pack0.x);

    float roughness = mat.roughness;
	float metallic = mat.metallic;

    vec3 bitangent = cross(inNormal, inTangent);
    mat3 tbn = mat3(inTangent, bitangent, inNormal);

#ifndef AMD_VIEWINDEX_WORKAROUND
    vec3 tViewDir = normalize((tbn * viewPos[gl_ViewIndex].xyz) - (tbn * inWorldPos.xyz));
#else
    vec3 tViewDir = normalize((tbn * viewPos[0].xyz) - (tbn * inWorldPos.xyz));
#endif

    vec2 tCoord = mat.heightmapIdx > -1 ? pm(inUV, tViewDir, mat) : inUV;
	
    vec4 albedoCol = texture(tex2dSampler[mat.albedoTexIdx], tCoord) * vec4(mat.albedoColor, 1.0);
	
	vec3 f0 = mix(vec3(0.04), albedoCol.rgb, metallic);
	vec3 lo = vec3(0.0);

    vec3 normal = mat.normalTexIdx > -1 ? getNormalMapNormal(mat, tCoord, tbn) : inNormal;
	
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
		lo += shadowIntensity * calculateLighting(i, viewDir, f0, metallic, roughness, albedoCol.rgb, normal);
	}

    if (mat.alphaCutoff > 0.0f) {
        albedoCol.a = (albedoCol.a - mat.alphaCutoff) / max(fwidth(albedoCol.a), 0.0001) + 0.5;
    }

    float finalAlpha = mat.alphaCutoff > 0.0f ? albedoCol.a : 1.0f;
#if 0//def BLINN_PHONG
    FragColor = vec4(lo, finalAlpha);
#else
	FragColor = vec4(lo + calcAmbient(f0, roughness, viewDir, metallic, albedoCol.xyz, normal) + mat.emissiveColor, finalAlpha);
#endif

    if (ENABLE_PICKING && doPicking == 1) {
        handleEditorPicking();
    }
}
#endif
