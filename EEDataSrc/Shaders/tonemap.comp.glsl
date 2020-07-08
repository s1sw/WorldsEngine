#version 450
layout (binding = 0, rgba8) uniform writeonly image2D resultImage;
layout (binding = 1, rgba16f) uniform readonly image2D hdrImage;
layout (binding = 2) uniform sampler2D motionVectors;
layout (binding = 3) uniform sampler2D lastFrame;
layout (local_size_x = 16, local_size_y = 16) in;

layout (push_constant) uniform Options {
	// 1 - Enable TAA
	// 2 - Enable motion blur
	// 4 - Debug motion vectors
	uint options;
};

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

vec3 InverseTonemap(vec3 x)
{
	return ( (x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F) ) - E/F;
}


vec3 sampleAt(vec2 coord)
{
	return imageLoad(hdrImage, ivec2(clamp(coord, vec2(0.0), vec2(1.0)) * textureSize(motionVectors, 0))).xyz;
}

vec3 sampleLast(vec2 coord) {
	return textureLod(lastFrame, coord, 0).xyz;
}

void main()
{
	vec3 col = imageLoad(hdrImage, ivec2(gl_GlobalInvocationID.xy)).xyz;
	
	vec2 texelSize = vec2(1.0) / textureSize(motionVectors, 0);
	vec2 texCoord = gl_GlobalInvocationID.xy * texelSize;
	vec2 motionVector = texture(motionVectors, texCoord).xy * 0.5;
	
	if ((options & 4) == 4) {
		imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4(abs(motionVector), 0.0, 1.0));
		return;
	}
	
	if ((options & 2) == 2) {
		vec2 adjustedMotionVector = motionVector;
		float speed = length(abs(adjustedMotionVector) / texelSize);
		int nSamples = clamp(int(speed), 1, 256);
		vec2 lastOnScreen = vec2(texCoord);
		
		for (int i = 1; i < nSamples; ++i) {
			vec2 offset = adjustedMotionVector * (float(i) / float(nSamples - 1) - 0.5);
			vec2 sampleLoc = texCoord + offset;
			if(any(greaterThan(sampleLoc, vec2(1.0))) || any(lessThan(sampleLoc, vec2(0.0)))) {
				sampleLoc = lastOnScreen;
			} else {
				lastOnScreen = sampleLoc;
			}
			
			col += sampleAt(sampleLoc);
		}
		
		col /= float(nSamples);
	}
	
	col *= 16.0;
	
	float exposureBias = 1.0;
	vec3 curr = Uncharted2Tonemap(exposureBias * col);
	
	vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
	vec3 newCol = curr * whiteScale;
	newCol = pow(newCol, vec3(1/2.2));
	
	if ((options & 1) == 1) {
		vec2 prevUv = texCoord - motionVector;
		vec3 lastFrameCol = textureLod(lastFrame, prevUv, 0).xyz;
		
		bool tooQuick = any(greaterThan(abs(motionVector), vec2(1.0) * texelSize));
		bool prevOutOfView = any(greaterThan(prevUv, vec2(1.0))) || any(lessThan(prevUv, vec2(0.0)));
		if(tooQuick || prevOutOfView) {
			lastFrameCol = newCol;
		}
		
		imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4((newCol * 0.25 + lastFrameCol * 0.75), 1.0));
	} else {
		imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), vec4(newCol, 1.0));
	}
}