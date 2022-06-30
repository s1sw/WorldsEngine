#version 450

layout (location = 0) in vec2 in_Position;
layout (location = 1) in vec2 in_UV;
layout (location = 2) in vec4 in_Color;

layout (location = 0) out vec2 out_UV;
layout (location = 1) out vec4 out_Color;

layout (push_constant) uniform PC
{
    vec2 Scale;
    vec2 Translate;
    uint TextureID;
};

vec4 toLinear(vec4 sRGB)
{
    bvec3 cutoff = lessThan(sRGB.rgb, vec3(0.04045));
    vec3 higher = pow((sRGB.rgb + vec3(0.055))/vec3(1.055), vec3(2.4));
    vec3 lower = sRGB.rgb/vec3(12.92);

    return vec4(mix(higher, lower, cutoff), sRGB.a);
}

void main()
{
    out_UV = vec2(in_UV.x, in_UV.y);
    out_Color = toLinear(in_Color);
    //out_Color = vec4(pow(in_Color.xyz, vec3(2.2)), in_Color.w);
    //out_Color = in_Color;
    gl_Position = vec4(in_Position * Scale + Translate, 0.0, 1.0);
}