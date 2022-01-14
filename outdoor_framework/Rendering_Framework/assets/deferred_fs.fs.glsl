#version 430 core

layout (location = 0) out vec4 fragColor

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
uniform sampler2D tex_vertex;
uniform sampler2D tex_normal;

void main()
{
	vec4 result = vec4(0, 0, 0, 1);
	vec3 diffuse = texelFetch(tex_diffuse, ivec2(gl_FragCoord.xy), 0).rgb;
	vec3 ambient = texelFetch(tex_ambient, ivec2(gl_FragCoord.xy), 0).rgb;
	vec3 specular = texelFetch(tex_specular, ivec2(gl_FragCoord.xy), 0).rgb;
	vec3 position = texelFetch(tex_vertex, ivec2(gl_FragCoord.xy), 0).rgb;
	vec3 normal = texelFetch(tex_normal, ivec2(gl_FragCoord.xy), 0).rgb;
}