#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#define PI 3.1415926535

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec4 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inShadowPos;
layout(location = 5) in float inDepth;

layout(constant_id = 0) const bool ENABLE_PICKING = false;
layout(constant_id = 1) const bool FACE_PICKING = false;

const int LT_POINT = 0;
const int LT_SPOT = 1;
const int LT_DIRECTIONAL = 2;

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
	// (metallic, roughness, albedo texture index, unused)
	vec4 pack0;
	// (albedo color rgb, unused)
	vec4 pack1;
};

layout(std140, binding = 2) uniform MaterialSettingsBuffer {
    Material materials[256];
};

layout (binding = 4) uniform sampler2D albedoSampler[];
layout (binding = 5) uniform sampler2DShadow shadowSampler;

layout(std430, binding = 6) buffer PickingBuffer {
    uint depth;
    uint objectID;
    uint doPicking;
} pickBuf;

layout(push_constant) uniform PushConstants {
	vec4 viewPos;
	vec4 texScaleOffset;
    // (x: model matrix index, y: material index, z: vp index, w: object id)
	ivec4 ubIndices;
    ivec2 pixelPickCoords;
};

float ndfGGXDistribution(float cosLh, float roughness) {
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;

    float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (PI * denom * denom);
}

float calcK(float roughness) {
    return ((roughness + 1.0) * (roughness + 1.0)) / 8.0;
}

float schlickG1(vec3 v, vec3 n, float k) {
    float ndotv = dot(n, v);
    return ndotv / (ndotv * (1.0 - k)) + k;
}

float schlickG(vec3 l, vec3 v, vec3 h, vec3 n, float roughness) {
    float k = calcK(roughness);
    //return schlickG1(l, n, k) * schlickG1(v, n, k);

    float NdotL = max(dot(n, l), 0.0);
    float NdotV = max(dot(n, v), 0.0);

    float r2 = roughness * roughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r2 + (1.0 - r2) * (NdotL * NdotL)));
	float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r2 + (1.0 - r2) * (NdotV * NdotV)));
	return attenuationL * attenuationV;
}

vec3 fresnelSchlick(vec3 F0, float cosTheta) {
    float a = 1.0 - cosTheta;
    return F0 + (1.0 - F0) * a * a * a * a * a;
}

vec3 calculateLighting(int lightIdx, vec3 viewDir, vec3 f0, float metallic, float roughness) {
	Light light = lights[lightIdx];
	int lightType = int(light.pack0.w);
    vec3 radiance = light.pack0.xyz;
    vec3 L = vec3(0.0f, 0.0f, 0.0f);
	vec3 lightPos = light.pack2.xyz;
	
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
    roughness = clamp(roughness + 0.05f, 0.0, 1.0);

    vec3 halfway = normalize(viewDir + L);
    vec3 norm = normalize(inNormal);

    float NdotV = max(dot(norm, viewDir), 0.0);
    float NdotL = max(dot(norm, L), 0.0);
    float NdotH = max(dot(norm, halfway), 0.0);

    //vec3 spec = specBrdf(halfway, normalize(viewDir), L, norm, vec3(0.04), roughness);

    float D = ndfGGXDistribution(max(NdotH, 0.001), roughness);
    float G = schlickG(L, viewDir, halfway, norm, roughness);
    vec3 F = fresnelSchlick(f0, NdotV);

    vec3 spec = D * F * G / max(4.0 * NdotL * NdotV, 0.001);
    vec3 diffuse = (1.0 - F);

    return NdotL * radiance * (diffuse + spec);
}

void main() {
    Material mat = materials[ubIndices.y];
	int lightCount = int(pack0.x);
	
	float metallic = mat.pack0.x;
	float roughness = mat.pack0.y;
	vec3 albedoColor = mat.pack1.rgb * texture(albedoSampler[int(mat.pack0.z)], inUV).rgb;
	
	vec3 viewDir = normalize(viewPos.xyz - inWorldPos.xyz);
	
	vec3 f0 = vec3(0.04f, 0.04f, 0.04f);
    f0 = mix(f0, albedoColor, metallic);
	vec3 lo = vec3(0.05);
	
    vec4 lightspacePos = inShadowPos;
    for (int i = 0; i < lightCount; i++) {
		vec3 cLighting = calculateLighting(i, viewDir, f0, metallic, roughness);
		if (int(lights[i].pack0.w) == LT_DIRECTIONAL) {
			float depth = (lightspacePos.z / lightspacePos.w) - 0.001;
			vec2 texReadPoint = (lightspacePos.xy * 0.5 + 0.5);
			
			if (texReadPoint.x > 0.0 && texReadPoint.x < 1.0 && texReadPoint.y > 0.0 && texReadPoint.y < 1.0 && depth < 1.0 && depth > 0.0) {
                float texelSize = 1.0 / textureSize(shadowSampler, 0).x;
				texReadPoint.y = 1.0 - texReadPoint.y;
                float shadowIntensity = 0.0;
                const int shadowSamples = 1;
                const float divVal = ((shadowSamples * 2) + 1) * ((shadowSamples * 2) + 1);
                for (int x = -shadowSamples; x < shadowSamples; x++)
                for (int y = -shadowSamples; y < shadowSamples; y++)
				    shadowIntensity += texture(shadowSampler, vec3(texReadPoint + vec2(x, y) * texelSize, depth)).x;
                shadowIntensity /= divVal;
                cLighting *= shadowIntensity;
			}
		}
		lo += cLighting;
	}
	
	// TODO: Ambient stuff - specular cubemaps + irradiance
	
	FragColor = vec4(lo * albedoColor, 1.0);

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