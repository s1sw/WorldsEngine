#version 450
layout(constant_id = 0) const int NUM_MSAA_SAMPLES = 4;

layout (binding = 0, rgba8) uniform writeonly image2D resultImage;
layout (binding = 1) uniform sampler2DMS hdrImage;

layout (local_size_x = 16, local_size_y = 16) in;


#define INVERSE_TONEMAP_RESOLVE 0

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

vec3 inverseToneMap(vec3 x) {
 	return (sqrt((4*x-4*x*x)*A*D*F*F*F+((4*x-4)*A*D*E+B*B*C*C+(2*x-2)*B*B*C+(x*x-2*x+1)*B*B)*F*F+((2-2*x)*B*B-2*B*B*C)*E*F+B*B*E*E)+((1-x)*B-B*C)*F+B*E)/(2*x*A*F-2*A*E);
}

vec3 tonemapCol(vec3 col, vec3 whiteScale) {
	col *= 16.0;
	
	float exposureBias = 0.3;
	vec3 curr = Uncharted2Tonemap(exposureBias * col);

	return pow(curr * whiteScale, vec3(1/2.2));
}

vec3 toneMap(vec3 x) {
 //return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F)-E/F);
 return 1.0-((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F)-E/F);
}

void main() {
	vec3 acc = vec3(0.0);
	vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
	for (int i = 0; i < NUM_MSAA_SAMPLES; i++) {
		#if INVERSE_TONEMAP_RESOLVE
		acc += toneMap(texelFetch(hdrImage, ivec2(gl_GlobalInvocationID.xy), i).xyz);
		#else
		acc += tonemapCol(texelFetch(hdrImage, ivec2(gl_GlobalInvocationID.xy), i).xyz, whiteScale);
		#endif
	}

	if (any(lessThan(acc, vec3(0.0)))) acc = vec3(1.0, 0.0, 0.0);


#if INVERSE_TONEMAP_RESOLVE
	acc = inverseToneMap(acc / float(NUM_MSAA_SAMPLES));

	//imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4(tonemapCol(texelFetch(hdrImage, ivec2(gl_GlobalInvocationID.xy), 0).xyz, whiteScale), 1.0));//vec4(acc / float(NUM_MSAA_SAMPLES), 1.0));
	imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4(tonemapCol(acc, whiteScale), 1.0));
#else
	imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4(acc / float(NUM_MSAA_SAMPLES), 1.0));
#endif
}