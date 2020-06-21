#version 450
layout (binding = 0, rgba8) uniform writeonly image2D resultImage;
layout (binding = 1, rgba16f) uniform readonly image2D hdrImage;
layout (local_size_x = 16, local_size_y = 16) in;

float A = 0.15;
float B = 0.50;
float C = 0.10;
float D = 0.20;
float E = 0.02;
float F = 0.30;
float W = 11.2;

vec3 Uncharted2Tonemap(vec3 x)
{
   return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main()
{
	vec3 col = imageLoad(hdrImage, ivec2(gl_GlobalInvocationID.xy)).xyz;
	
	col *= 16.0;
	
	float exposureBias = 1.0;
	vec3 curr = Uncharted2Tonemap(exposureBias * col);
	
	vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
	vec3 newCol = curr * whiteScale;
	newCol = pow(newCol, vec3(1/2.2));
	
	imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4(newCol, 1.0));
}