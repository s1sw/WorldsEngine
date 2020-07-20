#version 450
layout (binding = 0, rgba8) uniform writeonly image2D resultImage;
layout (binding = 1) uniform sampler2DMS hdrImage;
layout (binding = 2) uniform sampler2D imguiImage;
layout (local_size_x = 16, local_size_y = 16) in;

layout(constant_id = 0) const int NUM_MSAA_SAMPLES = 4;

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
	
	float exposureBias = 1.0;
	vec3 curr = Uncharted2Tonemap(exposureBias * col);

	return pow(curr * whiteScale, vec3(1/2.2));
}

void main() {
	vec3 acc = vec3(0.0);
	vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
	for (int i = 0; i < NUM_MSAA_SAMPLES; i++) {
		acc += tonemapCol(texelFetch(hdrImage, ivec2(gl_GlobalInvocationID.xy), i).xyz, whiteScale);
	}

	vec4 imguiCol = texture(imguiImage, vec2(gl_GlobalInvocationID.xy) / textureSize(imguiImage, 0).xy);
	vec3 sceneCol = acc / float(NUM_MSAA_SAMPLES);
	sceneCol *= 1.0 - imguiCol.a;
	imguiCol.xyz *= imguiCol.a;
	imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4(sceneCol + imguiCol.xyz, 1.0));
}