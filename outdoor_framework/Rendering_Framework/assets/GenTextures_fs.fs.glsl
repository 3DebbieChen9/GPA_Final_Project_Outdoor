#version 430 core

layout(location = 0) out vec4 diffuse_color;
layout(location = 1) out vec4 ambient_color;
layout(location = 2) out vec4 specular_color;
layout(location = 3) out vec4 ws_position;
layout(location = 4) out vec4 ws_normal;
layout(location = 5) out vec4 ws_tangent;

in VS_OUT{
	vec3 N; // model space normal
	vec2 texcoord;
	vec3 FragPos;
	vec3 ps;
	vec3 nm;
	vec3 ambient_color;
	vec3 specular_color;
	vec3 diffuse_color; // diffuse color from mtl
	vec3 tangent;
} fs_in;

uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;

uniform vec3 light_pos;
uniform vec3 view_pos;

uniform bool ubHasNormalMap;
uniform bool ubUseNormalMap;

void main()
{
	diffuse_color = vec4(texture(diffuseTexture, fs_in.texcoord).rgb, 1.0);
	ambient_color = vec4(fs_in.ambient_color, 1.0);
	specular_color = vec4(fs_in.specular_color, 1.0);
	ws_position = vec4(fs_in.ps, 1.0);
	if (ubHasNormalMap) {
		if (ubUseNormalMap) {
			ws_normal = vec4(texture(normalTexture, fs_in.texcoord).rgb, 1.0);
		}
		else {
			ws_normal = vec4(fs_in.nm / 2 + 1.0, 1.0);
		}
	}
	else {
		ws_normal = vec4(fs_in.nm / 2 + 1.0, 1.0);
	}
	ws_tangent = vec4(fs_in.tangent, 1.0);
	
}