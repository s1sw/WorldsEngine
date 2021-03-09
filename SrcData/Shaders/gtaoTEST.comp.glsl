#version 450
layout (binding = 0, rgba8) uniform image2D resultImage;
layout (local_size_x = 16, local_size_y = 16) in;

layout (push_constant) uniform PC {
    float aspect;
	float angleOffset;
	float spacialOffset;
	vec2 viewsizediv;
	vec2 viewSize;
};

void main()
{
	imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4(0.7575));
}