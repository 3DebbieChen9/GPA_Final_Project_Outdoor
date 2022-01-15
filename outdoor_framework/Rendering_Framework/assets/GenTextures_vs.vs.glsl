#version 430 core

layout(location = 0) in vec3 iv3position;
layout(location = 1) in vec2 iv2tex_coord;
layout(location = 2) in vec3 iv3normal;
layout(location = 3) in vec3 iv3tangent;

out VS_OUT{
	vec3 N; // model space normal
	vec2 texcoord;
	vec3 FragPos;
	vec3 ps;
	vec3 nm;
	vec3 ambient_color;
	vec3 specular_color;
	vec3 diffuse_color; // diffuse color from mtl
	vec3 tangent;
} vs_out;

uniform mat4 um4m;
uniform mat4 um4v;
uniform mat4 um4p;
uniform vec3 uv3Ambient;
uniform vec3 uv3Diffuse;
uniform vec3 uv3Specular;

void main() {
	gl_Position = um4p * um4v * um4m * vec4(iv3position, 1.0);
	vs_out.nm = mat3(um4m) * iv3normal;
	vs_out.ps = (um4m * vec4(iv3position, 1.0)).xyz;
	vs_out.N = iv3normal;
	vs_out.FragPos = vec3(um4m * vec4(iv3position, 1.0));
	vs_out.texcoord = iv2tex_coord;
	vs_out.tangent = mat3(um4m) * iv3tangent;
	vs_out.diffuse_color = uv3Diffuse;
	vs_out.specular_color = uv3Specular;
	vs_out.ambient_color = uv3Ambient;
}



