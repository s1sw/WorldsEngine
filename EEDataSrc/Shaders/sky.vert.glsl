#version 450
layout(location = 0) out vec3 pos;
layout(location = 1) out vec3 fsun;

layout(push_constant) uniform PushConst {
	mat4 proj;
	mat3 view;
	vec3 sunDir;
};


const vec2 data[4] = vec2[](
	vec2(-1.0,  1.0), vec2(-1.0, -1.0),
	vec2( 1.0,  1.0), vec2( 1.0, -1.0));

void main()
{
	gl_Position = vec4(data[gl_VertexID], 0.0, 1.0);
	pos = transpose(view) * (inverse(proj) * gl_Position).xyz;
	fsun = vec3(0.0, sin(time * 0.01), cos(time * 0.01));
}