#version 430 core

layout(location = 0) in vec3 iv3position;
layout(location = 1) in vec2 iv2tex_coord;
layout(location = 2) in vec3 iv3normal;
layout(location = 3) in vec3 iv3tangent;

uniform mat4 um4m;
uniform mat4 um4v;
uniform mat4 um4p;


out VS_OUT
{
	vec2 texcoord;
	vec3 ws_coords;
	vec3 normal;
	vec3 tangent;
} vs_out;

void main()
{
	vec4 P = vec4(iv3position, 1.0);
	mat4 mv = um4v * um4m;
	gl_Position = um4p * mv * P;

	vs_out.ws_coords = (um4m * P).xyz;
	vs_out.normal = normalize(mat3(um4m) * iv3normal);
	vs_out.texcoord = iv2tex_coord;
	vs_out.tangent = iv3tangent;
	
}