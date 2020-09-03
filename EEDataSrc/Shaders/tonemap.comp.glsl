#version 450
layout (binding = 0, rgba8) uniform writeonly image2D resultImage;
layout (binding = 1) uniform sampler2DMSArray hdrImage;
layout (local_size_x = 16, local_size_y = 16) in;

layout(constant_id = 0) const int NUM_MSAA_SAMPLES = 4;
layout(push_constant) uniform PushConstants {
	int idx;
};

float A = 0.15;
float B = 0.50;
float C = 0.10;
float D = 0.20;
float E = 0.02;
float F = 0.30;
float W = 11.2;

vec3 Uncharted2Tonemap(vec3 x) {
   return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 InverseTonemap(vec3 x) {
	return ( (x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F) ) - E/F;
}

vec3 tonemapCol(vec3 col, vec3 whiteScale) {
	col *= 16.0;
	
	float exposureBias = 0.75;
	vec3 curr = Uncharted2Tonemap(exposureBias * col);

	return pow(curr * whiteScale, vec3(1/2.2));
}

void main() {
	vec3 acc = vec3(0.0);
	vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
	for (int i = 0; i < NUM_MSAA_SAMPLES; i++) {
		acc += tonemapCol(texelFetch(hdrImage, ivec3(gl_GlobalInvocationID.xy, idx), i).xyz, whiteScale);
	}

	imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4(acc / float(NUM_MSAA_SAMPLES), 1.0));
}