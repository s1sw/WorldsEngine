#version 450
layout (local_size_x = 16, local_size_y = 16) in;
layout (binding = 0) uniform sampler2D mipChain;
layout (binding = 1, rgba16f) uniform writeonly image2D img;

void main() {
    vec2 size = textureSize(mipChain, 0);
    vec3 val = textureLod(mipChain, (vec2(gl_GlobalInvocationID.xy) + 0.5) / size, 0).xyz;
    
    imageStore(img, ivec2(gl_GlobalInvocationID.xy), vec4(val, 1.0));
}
