#version 430 core

layout(location = 0) out vec4 colorDiffuse; // Diffuse map
layout(location = 1) out vec4 colorAmbient; // Ambient map
layout(location = 2) out vec4 colorSpecular; // Specular map
layout(location = 3) out vec4 colorVertex; // World Space Vertex map
layout(location = 4) out vec4 colorNormal; // Normal map

in VS_OUT
{
	vec2 texcoord;
	vec3 ws_coords;
	vec3 normal;
	vec3 tangent;
} fs_in;

uniform sampler2D tex_diffuse;
uniform sampler2D tex_ambient;
uniform sampler2D tex_specular;

void main()
{
	colorDiffuse = vec4(texture(tex_diffuse, fs_in.texcoord), 1.0f);
	colorAmbient = vec4(texture(tex_ambient, fs_in.texcoord), 1.0f);
	colorSpecular = vec4(texture(tex_specular, fs_in.texcoord), 1.0f);
	colorVertex = vec4(fs_in.ws_coords, 1.0);
	colorNormal = vec4(normalize(fs_in.normal), 0.0);
}