#version 450
layout(location = 0) out vec3 pos;

layout(push_constant) uniform PushConst {
	mat4 proj;
	mat4 view;
};

void main() {
	int tri = gl_VertexID / 3;
    int idx = gl_VertexID % 3;
    int face = tri / 2;
    int top = tri % 2;

    int dir = face % 3;
    int pos = face / 3;

    int nz = dir >> 1;
    int ny = dir & 1;
    int nx = 1 ^ (ny | nz);

    vec3 d = vec3(nx, ny, nz);
    float flip = 1 - 2 * pos;

    vec3 n = flip * d;
    vec3 u = -d.yzx;
    vec3 v = flip * d.zxy;

    float mirror = -1 + 2 * top;
    vec3 xyz = n + mirror*(1-2*(idx&1))*u + mirror*(1-2*(idx>>1))*v;

	pos = xyz;
	gl_Position = proj * view * vec4(pos, 0.0);
}