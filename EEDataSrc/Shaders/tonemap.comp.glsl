#version 450
layout (binding = 0, rgba8) uniform writeonly image2D resultImage;
layout (binding = 1) uniform sampler2DMSArray hdrImage;
layout (binding = 2) uniform sampler2DArray gtaoImage;
layout (local_size_x = 16, local_size_y = 16) in;

layout(constant_id = 0) const int NUM_MSAA_SAMPLES = 4;
layout(push_constant) uniform PushConstants {
    float aoIntensity;
	int idx;
    float exposureBias;
};

float A = 0.15;
float B = 0.50;
float C = 0.10;
float D = 0.20;
float E = 0.02;
float F = 0.30;
float W = 11.2;

vec3 ACESFilm(vec3 x) {
    x *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), vec3(0.0), vec3(1.0));
}

vec3 Uncharted2Tonemap(vec3 x) {
   return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 InverseTonemap(vec3 x) {
	return ( (x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F) ) - E/F;
}

vec3 tonemapCol(vec3 col, vec3 whiteScale) {
	col *= 16.0;
	
	vec3 curr = Uncharted2Tonemap(exposureBias * col);

	return curr * whiteScale;
}

void main() {
	vec3 acc = vec3(0.0);
	vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
    float aoVal = (1.0 - aoIntensity) + (texelFetch(gtaoImage, ivec3(gl_GlobalInvocationID.xy, idx), 0).x * aoIntensity);
	for (int i = 0; i < NUM_MSAA_SAMPLES; i++) {
        vec3 raw = texelFetch(hdrImage, ivec3(gl_GlobalInvocationID.xy, idx), i).xyz * aoVal;
		acc += tonemapCol(raw, whiteScale);
        //acc += ACESFilm(raw * 4.0);
	}

    // debugging checks for NaN and negatives
    /*
	if (any(lessThan(acc, vec3(0.0)))) acc = vec3(1.0, 0.0, 0.0);
	if (any(isnan(acc))) acc = vec3(1.0, 0.0, 1.0);	
    */
	
    vec3 final = pow(acc / float(NUM_MSAA_SAMPLES), vec3(1 / 2.2));

	imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4(final, 1.0));
}
