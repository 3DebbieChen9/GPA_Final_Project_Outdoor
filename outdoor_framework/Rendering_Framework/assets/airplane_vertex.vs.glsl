#version 430 core

layout(location = 0) in vec3 iv3position;
layout(location = 1) in vec2 iv2tex_coord;
layout(location = 2) in vec3 iv3normal;
layout(location = 3) in vec3 iv3tangent;

uniform mat4 um4m;
uniform mat4 um4v;
uniform mat4 um4p;
uniform vec3 uv3LightPos;

out VS_OUT
{
	vec3 N; // eye space normal
	vec3 L; // eye space light vector
	vec3 V; // eye space view vector
	vec2 texcoord;
	vec3 eyeDir;
	vec3 lightDir;
	//vec3 ws_coords;
	//vec3 normal;
	//vec3 tangent;
} vs_out;

void main()
{
	// Calculate view-space coordinate
	vec4 P = um4v * um4m * vec4(iv3position, 1.0);
	// Eye space to tangent space TBN
	vec3 T = normalize(mat3(um4v * um4m) * iv3tangent);
	vec3 N = normalize(mat3(um4v * um4m) * iv3normal);
	vec3 B = cross(N, T);
	vec3 L = uv3LightPos - P.xyz;
	vec3 V = -P.xyz;

	vs_out.N = mat3(um4v * um4m) * iv3normal;
	vs_out.L = uv3LightPos - P.xyz;
	vs_out.V = -P.xyz;

	vs_out.texcoord = iv2tex_coord;
	vs_out.eyeDir = normalize(vec3(dot(V, T), dot(V, B), dot(V, N)));
	vs_out.lightDir = normalize(vec3(dot(L, T), dot(L, B), dot(L, N)));
	
	//vs_out.ws_coords = (um4m * iv3position).xyz;
	//vs_out.normal = mat3(um4m) * iv3normal;
	//vs_out.tangent = iv3tangent;

	// Calculate the clip-space position of each vertex
	gl_Position = um4p * P;
}