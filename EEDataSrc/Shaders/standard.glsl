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
	vec4 pack0;
	// (albedo color rgb, unused)
	vec4 pack1;
};

layout(std140, binding = 2) uniform MaterialSettingsBuffer {
    Material materials[256];
};

layout(std140, binding = 3) uniform ModelMatrices {
	mat4 modelMatrices[1024];
};

layout (binding = 4) uniform sampler2D albedoSampler[];
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
    // (x: model matrix index, y: material index, z: vp index, w: object id)
	ivec4 ubIndices;
    ivec2 pixelPickCoords;
};

#ifdef VERTEX
void main() {
	mat4 model = modelMatrices[ubIndices.x];
    gl_Position = projection[ubIndices.z + gl_ViewIndex] * view[ubIndices.z + gl_ViewIndex] * model * vec4(inPosition, 1.0); // Apply MVP transform
	
    outUV = (inUV * texScaleOffset.xy) + texScaleOffset.zw;
    outNormal = normalize(model * vec4(inNormal, 0.0)).xyz;
    outTangent = normalize(model * vec4(inTangent, 0.0)).xyz;
    outWorldPos = (model * vec4(inPosition, 1.0));
	outShadowPos = shadowmapMatrix * model * vec4(inPosition, 1.0);
	outDepth = gl_Position.z / gl_Position.w;
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
}
#endif

#ifdef FRAGMENT
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
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
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;

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
	vec3 lightPos = light.pack2.xyz;

    if (dot(normal, viewDir) < 0.0)
        normal = -normal;
	
    if (lightType == LT_POINT) {
        L = lightPos - inWorldPos.xyz;
        // dot(L, L) = length(L) squared
        radiance *= 1.0 / dot(L, L);
		L = normalize(L);
    } else if (lightType == LT_SPOT) {
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

    vec3 diffuse = kd * albedoColor;
    vec3 lPreShadow = (specular + diffuse) * radiance * cosLi;

    return lPreShadow;
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

void main() {
    Material mat = materials[ubIndices.y];

    float roughness = mat.pack0.y;
	vec3 viewDir = normalize(viewPos[gl_ViewIndex].xyz - inWorldPos.xyz);

	int lightCount = int(pack0.x);
	
	float metallic = mat.pack0.x;
	
    vec4 albedoTex = texture(albedoSampler[int(mat.pack0.z)], inUV);
	vec3 albedoColor = mat.pack1.rgb * albedoTex.rgb;
	
	
	vec3 f0 = vec3(0.04);
    f0 = mix(f0, albedoColor, metallic);
	vec3 lo = vec3(0.0);

    vec3 normal = inNormal;

    if (mat.pack0.w > -1.0f) {
        vec3 bitangent = cross(inNormal, inTangent);

        mat3 tbn = mat3(inTangent, bitangent, inNormal);

        vec3 texNorm = normalize(decodeNormal(texture(albedoSampler[(int(mat.pack0.w))], inUV).xy));
        normal = normalize(tbn * texNorm);
        // (normal * 2.0) - 1.0
        //FragColor = vec4((normal * 0.25) + 0.25, 1.0);
        //return;
    }
	
    vec4 lightspacePos = inShadowPos;
    for (int i = 0; i < lightCount; i++) {
		vec3 cLighting = calculateLighting(i, viewDir, f0, metallic, roughness, albedoColor, normal);
		if (int(lights[i].pack0.w) == LT_DIRECTIONAL) {
			float depth = (lightspacePos.z / lightspacePos.w) - 0.001;
			vec2 texReadPoint = (lightspacePos.xy * 0.5 + 0.5);
			
			if (texReadPoint.x > 0.0 && texReadPoint.x < 1.0 && texReadPoint.y > 0.0 && texReadPoint.y < 1.0 && depth < 1.0 && depth > 0.0) {
                float texelSize = 1.0 / textureSize(shadowSampler, 0).x;
				texReadPoint.y = 1.0 - texReadPoint.y;
                float shadowIntensity = 0.0;
                const int shadowSamples = 2;
                const float divVal = ((shadowSamples * 2)) * ((shadowSamples * 2));
                for (int x = -shadowSamples; x < shadowSamples; x++)
                for (int y = -shadowSamples; y < shadowSamples; y++)
				    shadowIntensity += texture(shadowSampler, vec3(texReadPoint + vec2(x, y) * texelSize, depth)).x;
                shadowIntensity /= divVal;
                cLighting *= shadowIntensity;
			}
		}
		lo += cLighting;
	}

    // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.

    // ambient lighting (we now use IBL as the ambient term)
    vec3 F = fresnelSchlickRoughness(clamp(dot(inNormal, viewDir), 0.0, 1.0), f0, roughness);
    
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    const float MAX_REFLECTION_LOD = 11.0;
    vec3 R = reflect(-viewDir, normalize(inNormal));
    vec3 prefilteredColor = textureLod(cubemapSampler[0], R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf  = textureLod(brdfLutSampler, vec2(min(max(dot(inNormal, viewDir), 0.0), 0.95), roughness), 0.0).rg;
    vec3 specularAmbient = pow(prefilteredColor, vec3(2.2)) * (F * brdf.x + brdf.y);
    vec3 diffuseAmbient = textureLod(cubemapSampler[0], inNormal, MAX_REFLECTION_LOD * 0.5).xyz * 0.5 * albedoColor;
    vec3 ambient = kD * diffuseAmbient + specularAmbient;

    if (mat.pack1.w > 0.0f) {
        albedoTex.a = (albedoTex.a - mat.pack1.w) / max(fwidth(albedoTex.a), 0.0001) + 0.5;
        if (albedoTex.a < 0.5f) {
            //discard;
        }
    }

    //FragColor = vec4(metallic, roughness, 0.0, 1.0);
	//FragColor = vec4(lo, 1.0);
	FragColor = vec4(lo + ambient, mat.pack1.w > 0.0f ? albedoTex.a : 1.0f);

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
                        pickBuf.objectID = ubIndices.w;
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