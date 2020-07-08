layout (binding = 0, rgba8) uniform writeonly image2D resultImage;
layout (binding = 1, rgba8) uniform readonly image2D sdfImage;
layout (binding = 2, rgba8) uniform readonly image2D polyImage;
layout (binding = 3) uniform sampler2D polyDepth;
layout (local_size_x = 16, local_size_y = 16) in;

layout (push_constant) uniform PC {
	vec2 resolution;
};

float depth2dist(float depth, float near, float far)
{
	return (depth * far) + near;
}

void main()
{
	float2 uv = gl_GlobalInvocationID.xy / resolution;
	
	float depth = texture(polyDepth, uv).r;
	
}