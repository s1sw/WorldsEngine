#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_multiview : enable
#define PI 3.1415926535

#ifdef FRAGMENT
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
	mat4 view[8];
	mat4 projection[8];
    vec4 viewPos[8];
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
	Light lights[16];
};

struct Material {
	// (metallic, roughness, albedo texture index, normal texture index)
	//vec4 pack0;
    float metallic;
    float roughness;
    int albedoTexIdx;
    int normalTexIdx;
	// (albedo color rgb, alpha cutoff)
    vec3 albedoColor;
    float alphaCutoff;
	//vec4 pack1;
    float fresnelHackFactor;
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
    uint depth;
    uint objectID;
    uint doPicking;
} pickBuf;

layout(push_constant) uniform PushConstants {
	vec4 texScaleOffset;
    int modelMatrixIdx;
    int matIdx;
    int vpIdx;
    uint objectId;
    ivec2 pixelPickCoords;
};

#ifdef VERTEX
void main() {
	mat4 model = modelMatrices[modelMatrixIdx];
    gl_Position = projection[vpIdx + gl_ViewIndex] * view[vpIdx + gl_ViewIndex] * model * vec4(inPosition, 1.0); // Apply MVP transform
	
    outUV = (inUV * texScaleOffset.xy) + texScaleOffset.zw;
    outNormal = normalize(model * vec4(inNormal, 0.0)).xyz;
    outTangent = normalize(model * vec4(inTangent, 0.0)).xyz;
    outWorldPos = (model * vec4(inPosition, 1.0));
	outShadowPos = shadowmapMatrix * model * vec4(inPosition, 1.0);
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
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
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
    float cosLo = max(0.0f, dot(norm, viewDir));

    float NDF = ndfGGX(cosLh, roughness);
    float G = gaSchlickGGX(cosLi, cosLo, roughness);
    vec3 f = fresnelSchlick(f0, max(dot(halfway, viewDir), 0.0f));

    vec3 kd = mix(vec3(1.0) - f, vec3(0.0), metallic);

    vec3 numerator = NDF * G * f;
    float denominator = 4.0f * cosLo * cosLi;
    vec3 specular = numerator / max(denominator, 0.001f);

    return (specular + (kd * albedoColor)) * radiance * cosLi;
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

    vec3 specularAmbient = pow(textureLod(cubemapSampler[0], R, roughness * MAX_REFLECTION_LOD).rgb, vec3(2.2));

    vec2 brdf  = textureLod(brdfLutSampler, vec2(min(max(dot(normal, viewDir), 0.0), 0.95), roughness), 0.0).rg;
    float f90 = clamp(50.0 * f0.g, 0.0, 1.0);

    vec3 specularColor = (f0 * brdf.x) + (brdf.y * f90);

    float horizon = min(1.0 + dot(R, normal), 1.0);
    specularAmbient *= horizon * horizon;
    vec3 diffuseAmbient = textureLod(cubemapSampler[0], normal, MAX_REFLECTION_LOD * 0.5).xyz * 0.5 * albedoColor;

    return kD * diffuseAmbient + (specularAmbient * specularColor);
}

vec3 getNormalMapNormal(Material mat) {
    vec3 bitangent = cross(inNormal, inTangent);

    mat3 tbn = mat3(inTangent, bitangent, inNormal);

    vec3 texNorm = normalize(decodeNormal(texture(tex2dSampler[mat.normalTexIdx], inUV).xy));
    return normalize(tbn * texNorm);
}

void main() {
    //pickBuf.objectID = 0;
    Material mat = materials[matIdx];

	vec3 viewDir = normalize(viewPos[gl_ViewIndex].xyz - inWorldPos.xyz);

	int lightCount = int(pack0.x);

    float roughness = mat.roughness;
	float metallic = mat.metallic;
	
    vec4 albedoCol = texture(tex2dSampler[mat.albedoTexIdx], inUV) * vec4(mat.albedoColor, 1.0);
	
	vec3 f0 = mix(vec3(0.04), albedoCol.rgb, metallic);
	vec3 lo = vec3(0.0);

    vec3 normal = mat.normalTexIdx > -1 ? getNormalMapNormal(mat) : inNormal;
	
    for (int i = 0; i < lightCount; i++) {
        float shadowIntensity = 1.0;
		if (int(lights[i].pack0.w) == LT_DIRECTIONAL) {
			float depth = (inShadowPos.z / inShadowPos.w) - 0.0001;
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

	FragColor = vec4(lo + calcAmbient(f0, roughness, viewDir, metallic, albedoCol.xyz, normal), mat.alphaCutoff > 0.0f ? albedoCol.a : 1.0f);

    if (ENABLE_PICKING && pickBuf.doPicking == 1) {
        
        if (pixelPickCoords == ivec2(gl_FragCoord.xy)) {
            FragColor = vec4(1.0);
            // shitty gpu spinlock
            // it's ok, should rarely be contended
            // uint set = 0;

            //uint uiDepth = floatBitsToUint(inDepth);
            //atomicMin(pickBuf.depth, uiDepth);

            //if (pickBuf.depth == uiDepth) {
            //    atomicExchange(pickBuf.objectID, ubIndices.w);
            //}
             //pickBuf.doPicking = 0;

            uint d = floatBitsToUint(inDepth);
            uint current_d_or_locked = 0;
            do {
                // `z` is behind the stored z value, return immediately.
                if (d >= pickBuf.depth)
                    return;

                // Perform an atomic min. `current_d_or_locked` holds the currently stored
                // value.
                //picking_buffer.InterlockedMin(0, d, current_d_or_locked);
                current_d_or_locked = atomicMin(pickBuf.depth, d);
                // We rely on using the sign bit to indicate if the picking buffer is
                // currently locked. This means that this branch will only be entered if the
                // buffer is unlocked AND `d` is the less than the currently stored `d`. 
                if (d < int(current_d_or_locked)) {
                    uint last_d = 0;
                    // Attempt to acquire write lock by setting the sign bit.
                    last_d = atomicCompSwap(pickBuf.depth, d, floatBitsToUint(intBitsToFloat(-int(d))));
                    // This branch will only be taken if taking the write lock succeded.
                    if (last_d == d) {
                        // Update the object identity.
                        pickBuf.objectID = objectId;
                        uint dummy;
                        // Release write lock. 
                        //picking_buffer.InterlockedExchange(0, d, dummy);
                        atomicExchange(pickBuf.depth, d);
                    }
                }
            // Spin until write lock has been released.
            } while(int(current_d_or_locked) < 0);
        }
    }
}
#endif