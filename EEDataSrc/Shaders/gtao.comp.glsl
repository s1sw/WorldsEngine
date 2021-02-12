#version 450
layout (binding = 0) uniform writeonly image2DArray resultImage;
layout (binding = 1) uniform sampler2DMSArray inDepth;
layout (local_size_x = 16, local_size_y = 16) in;

// Ground truth-based ambient occlusion
// Implementation based on:
// Practical Realtime Strategies for Accurate Indirect Occlusion, Siggraph 2016
// Jorge Jimenez, Xianchun Wu, Angelo Pesce, Adrian Jarabo

// Implementation by /u/Kvaleya
// 2018-08-11

/*
	The only varibale needed from sharedUniformsBuffer is:
	vec2 viewsizediv = vec2(1.0 / sreenWidth, 1.0 / screenHeight)
*/

layout (push_constant) uniform PC {
    float aspect;
	float angleOffset;
	float spacialOffset;
    float SSAO_RADIUS;
	vec2 viewsizediv;
	vec2 viewSize;
    float SSAO_LIMIT;
    float SSAO_FALLOFF;
    float SSAO_THICKNESSMIX;
	mat4 invProj;
};

#define PI 3.1415926535897932384626433832795
#define PI_HALF 1.5707963267948966192313216916398

// [Eberly2014] GPGPU Programming for Games and Science
float GTAOFastAcos(float x)
{
#if 1 
	float res = -0.156583 * abs(x) + PI_HALF;
	res *= sqrt(1.0 - abs(x));
	return x >= 0 ? res : PI - res;
#else
    return acos(x);
#endif
}

float IntegrateArc(float h1, float h2, float n)
{
	float cosN = cos(n);
	float sinN = sin(n);
	return 0.25 * (-cos(2.0 * h1 - n) + cosN + 2.0 * h1 * sinN - cos(2.0 * h2 - n) + cosN + 2.0 * h2 * sinN);
}

vec3 Visualize_0_3(float x)
{
	const vec3 color0 = vec3(1.0, 0.0, 0.0);
	const vec3 color1 = vec3(1.0, 1.0, 0.0);
	const vec3 color2 = vec3(0.0, 1.0, 0.0);
	const vec3 color3 = vec3(0.0, 1.0, 1.0);
	vec3 color = mix(color0, color1, clamp(x - 0.0, 0.0, 1.0));
	color = mix(color, color2, clamp(x - 1.0, 0.0, 1.0));
	color = mix(color, color3, clamp(x - 2.0, 0.0, 1.0));
	return color;
}

vec3 GetCameraVec(vec2 uv)
{	
	// Returns the vector from camera to the specified position on the camera plane (uv argument), located one unit away from the camera
	// This vector is not normalized.
	// The nice thing about this setup is that the returned vector from this function can be simply multiplied with the linear depth to get pixel's position relative to camera position.
	// This particular function does not account for camera rotation or position or FOV at all (since we don't need it for AO)
	// TODO: AO is dependent on FOV, this function is not!
	// The outcome of using this simplified function is that the effective AO range is larger when using larger FOV
	// Use something more accurate to get proper FOV-independent world-space range, however you will likely also have to adjust the SSAO constants below
	return vec3(uv.x * -2.0 + 1.0, uv.y * 2.0 * aspect - aspect, 1.0);
}

//vec3 GetCameraVec(vec2 uv)
//{
//	return (proj * vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, 1.0, 0.0)).xyz;
//}

//vec3 reconstructPos(vec2 uv, float z) {
//	float x = uv.x * 2.0f - 1.0f;
//	float y = (1.0 - uv.y) * 2.0f - 1.0f;
//	vec4 position_s = vec4(x, y, z, 1.0f);
//	vec4 position_v = invVP * position_s;
//	return position_v.xyz / position_v.w;
//}

vec3 reconstructViewSpacePos(vec2 uv, float z) {
	vec4 clipSpacePos = vec4(uv * 2.0 - 1.0, 1.0 - z, 1.0);
	vec4 viewSpacePos = invProj * clipSpacePos;
	
	viewSpacePos /= viewSpacePos.w;
	
	return viewSpacePos.xyz;
}

//#define SSAO_LIMIT 5 
#define SSAO_SAMPLES 4 
//#define SSAO_RADIUS 0.25
//#define SSAO_FALLOFF 0.01
//#define SSAO_THICKNESSMIX 0.3
#define SSAO_MAX_STRIDE 32 

float sampleDepth(vec2 uv) 
{
	ivec2 tc = ivec2(clamp(ivec2(uv * viewSize), ivec2(0), viewSize));
	float dval = (texelFetch(inDepth, ivec3(tc, gl_GlobalInvocationID.z), 0).x);
	return (dval == 0.0 || dval == 1.0) ? 1000000.0 : (0.01 / dval);
}

float sampleDepthCS(vec2 uv)
{
	ivec2 tc = ivec2(clamp(ivec2(uv * viewSize), ivec2(0), viewSize));
	return texelFetch(inDepth, ivec3(tc, gl_GlobalInvocationID.z), 0).x;
}

void SliceSample(vec2 tc_base, vec2 aoDir, int i, float targetMip, vec3 ray, vec3 v, inout float closest)
{
	vec2 uv = tc_base + aoDir * i;
	float depth = sampleDepth(uv);
	// Vector from current pixel to current slice sample
	vec3 p = GetCameraVec(uv) * depth - ray;
	// Cosine of the horizon angle of the current sample
	float current = dot(v, normalize(p));
	// Linear falloff for samples that are too far away from current pixel
	float falloff = clamp((SSAO_RADIUS - length(p)) / SSAO_FALLOFF, 0.0, 1.0);
	if(current > closest)
		closest = mix(closest, current, falloff);
	// Helps avoid overdarkening from thin objects
	closest = mix(closest, current, SSAO_THICKNESSMIX * falloff);
}

vec3 getPosAt(vec2 uv) {
	return reconstructViewSpacePos(uv, sampleDepthCS(uv));
}

shared float depthVals[16][16];

void main() {
	vec2 tc_original = gl_GlobalInvocationID.xy * viewsizediv;
	
	// Depth of the current pixel
	float dhere = sampleDepth(tc_original);
	float actualDhere = texelFetch(inDepth, ivec3(gl_GlobalInvocationID.xyz), 0).x;
    depthVals[gl_LocalInvocationID.x][gl_LocalInvocationID.y] = actualDhere;
    memoryBarrierShared();
    barrier();
	if (actualDhere == 0.0 || actualDhere == 1.0) {
		imageStore(resultImage, ivec3(gl_GlobalInvocationID.xyz), vec4(1.0));
		return;
	}
	// Vector from camera to the current pixel's position
	vec3 ray = GetCameraVec(tc_original) * dhere;
	
	const float normalSampleDist = 1.0;
	
	// Calculate normal from the 4 neighbourhood pixels
	//vec2 uv = tc_original + vec2(viewsizediv.x * normalSampleDist, 0.0);
	//vec3 p1 = reconstructViewSpacePos(uv, sampleDepthCS(uv));
	//
	//uv = tc_original + vec2(0.0, viewsizediv.y * normalSampleDist);
	//vec3 p2 = reconstructViewSpacePos(uv, sampleDepthCS(uv));
	//
	//uv = tc_original + vec2(-viewsizediv.x * normalSampleDist, 0.0);
	//vec3 p3 = reconstructViewSpacePos(uv, sampleDepthCS(uv));
	
	//vec3 normal = cross(p3 - p1, p2 - p1);
	
	vec2 uv = tc_original + vec2(viewsizediv.x * normalSampleDist, 0.0);
	vec3 p1 = ray - GetCameraVec(uv) * sampleDepth(uv);
	
	uv = tc_original + vec2(0.0, viewsizediv.y * normalSampleDist);
	vec3 p2 = ray - GetCameraVec(uv) * sampleDepth(uv);
	
	uv = tc_original + vec2(-viewsizediv.x * normalSampleDist, 0.0);
	vec3 p3 = ray - GetCameraVec(uv) * sampleDepth(uv);
	
	uv = tc_original + vec2(0.0, -viewsizediv.y * normalSampleDist);
	vec3 p4 = ray - GetCameraVec(uv) * sampleDepth(uv);
	
	vec3 normal1 = normalize(cross(p1, p2));
	vec3 normal2 = normalize(cross(p3, p4));
	
	vec3 normal = normalize(normal1 + normal2);
	
	// Calculate the distance between samples (direction vector scale) so that the world space AO radius remains constant but also clamp to avoid cache trashing
	// viewsizediv = vec2(1.0 / sreenWidth, 1.0 / screenHeight)
	float stride = min((1.0 / length(ray)) * SSAO_LIMIT, SSAO_MAX_STRIDE);
	vec2 dirMult = viewsizediv.xy * stride;
	// Get the view vector (normalized vector from pixel to camera)
	vec3 v = normalize(-ray);
	
	// Calculate slice direction from pixel's position
	float dirAngle = (PI / 16.0) * (((int(gl_GlobalInvocationID.x) + int(gl_GlobalInvocationID.y) & 3) << 2) + (int(gl_GlobalInvocationID.x) & 3)) + angleOffset;
	vec2 aoDir = dirMult * vec2(sin(dirAngle), cos(dirAngle));
	
	// Project world space normal to the slice plane
	vec3 toDir = GetCameraVec(tc_original + aoDir);
	vec3 planeNormal = normalize(cross(v, -toDir));
	vec3 projectedNormal = normal - planeNormal * dot(normal, planeNormal);
	
	// Calculate angle n between view vector and projected normal vector
	vec3 projectedDir = normalize(normalize(toDir) + v);
	float n = GTAOFastAcos(dot(-projectedDir, normalize(projectedNormal))) - PI_HALF;
	
	// Init variables
	float c1 = -1.0;
	float c2 = -1.0;
	
	vec2 tc_base = tc_original + aoDir * (0.25 * ((int(gl_GlobalInvocationID.y) - int(gl_GlobalInvocationID.x)) & 3) - 0.375 + spacialOffset);
	
	const float minMip = 0.0;
	const float maxMip = 3.0;
	const float mipScale = 1.0 / 12.0;
	
	float targetMip = floor(clamp(pow(stride, 1.3) * mipScale, minMip, maxMip));
	
	// Find horizons of the slice
	for(int i = -1; i >= -SSAO_SAMPLES; i--)
	{
		SliceSample(tc_base, aoDir, i, targetMip, ray, v, c1);
	}
	
	for(int i = 1; i <= SSAO_SAMPLES; i++)
	{
		SliceSample(tc_base, aoDir, i, targetMip, ray, v, c2);
	}
	
	// Finalize
	float h1a = -GTAOFastAcos(c1);
	float h2a = GTAOFastAcos(c2);
	
	// Clamp horizons to the normal hemisphere
	float h1 = n + max(h1a - n, -PI_HALF);
	float h2 = n + min(h2a - n, PI_HALF);
	
	float visibility = mix(1.0, IntegrateArc(h1, h2, n), length(projectedNormal));
	
	imageStore(resultImage, ivec3(gl_GlobalInvocationID.xyz), vec4(vec3(visibility), 1.0));
}
