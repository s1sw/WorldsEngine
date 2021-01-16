#version 450
layout(constant_id = 0) const int NUM_MSAA_SAMPLES = 4;

layout (binding = 0) uniform readonly sampler2DMS inDepth;
layout (binding = 1, r32f) uniform writeonly image2D outDepth;

layout (local_size_x = 16, local_size_y = 16) in;
// "resolves" an msaa depth texture by taking the min of each sample

void main() {
	float depthVal = 1.0;
	
	for (int i = 0; i < NUM_MSAA_SAMPLES; i++) {
		depthVal = min(depthVal, texelFetch(inDepth, ivec2(gl_GlobalInvocationID.xy), i));
	}
	
	imageStore(outDepth, vec4(depthVal));
}