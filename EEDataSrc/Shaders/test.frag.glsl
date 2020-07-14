#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#define PI 3.1415926535

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec4 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUV;
layout(location = 4) in float inAO;
layout(location = 5) in vec4 inShadowPos;

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

layout(push_constant) uniform PushConstants {
	vec4 viewPos;
	vec4 texScaleOffset;
    // (x: model matrix index, y: material index, z: vp index)
	ivec4 ubIndices;
};

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;

    float num = NdotV;
    float denom = NdotV * (1.0f - k) + k;

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

    float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k) {
    return cosTheta / (cosTheta * (1.0 - k) + k);
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

    float cosLh = max(0.0f, dot(norm, halfway));
    float cosLi = max(0.0f, dot(norm, L));
    float cosLo = max(0.0f, dot(norm, viewDir));

    float NDF = ndfGGX(cosLh, roughness);
    float G = gaSchlickGGX(cosLi, cosLo, roughness);
	// Not 100% why the clamp is necessary here but we get random NaNs without it (likely cosTheta > 1.0 causing pow's x argument to be < 0)
    vec3 f = fresnelSchlick(f0, clamp(dot(halfway, viewDir), 0.0f, 1.0f));

    vec3 kd = mix(vec3(1, 1, 1) - f, vec3(0, 0, 0), 0.0);

    vec3 numerator = NDF * G * f;
    float denominator = 4.0f * cosLo * cosLi;
    vec3 specular = numerator / max(denominator, 0.001f);

    vec3 diffuse = kd;
    vec3 lPreShadow = (specular + diffuse) * (radiance * cosLi);

    return lPreShadow;
}

void main() {
    Material mat = materials[ubIndices.y];
	int lightCount = int(pack0.x);
	
	float metallic = mat.pack0.x;
	float roughness = mat.pack0.y;
	vec3 albedoColor = mat.pack1.rgb;
	
	vec3 viewDir = normalize(viewPos.xyz - inWorldPos.xyz);
	
	vec3 f0 = vec3(0.04f, 0.04f, 0.04f);
    f0 = mix(f0, albedoColor, metallic);
	vec3 lo = vec3(0.05) * inAO;
	
    vec4 lightspacePos = inShadowPos;
    for (int i = 0; i < lightCount; i++) {
		vec3 cLighting = calculateLighting(i, viewDir, f0, metallic, roughness);
		if (i == 0) {
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
	
	FragColor = vec4(lo * texture(albedoSampler[int(mat.pack0.z)], (inUV * texScaleOffset.xy) + texScaleOffset.zw).rgb, 1.0);
}