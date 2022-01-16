#version 430 core

#define TEXTURE_FINAL 5
#define TEXTURE_DIFFUSE 6
#define TEXTURE_AMBIENT 7
#define TEXTURE_SPECULAR 8
#define TEXTURE_WS_POSITION 9
#define TEXTURE_WS_NORMAL 10
#define TEXTURE_WS_TANGENT 11
#define TEXTURE_PHONG 12
#define TEXTURE_PHONGSHADOW 13
#define TEXTURE_BLOOMHDR 14
#define TEXTURE_BLOOM 15

layout(location = 0) out vec4 fragColor;

in VS_OUT
{
	vec2 texcoord;
} fs_in;

uniform int currentTex;

uniform sampler2D tex_diffuse;
uniform sampler2D tex_ambient;
uniform sampler2D tex_specular;
uniform sampler2D tex_ws_position;
uniform sampler2D tex_ws_normal;
uniform sampler2D tex_ws_tangent;
uniform sampler2D tex_phong;
uniform sampler2D tex_bloomHDR;
uniform sampler2D tex_phongShadow;
uniform sampler2D tex_bloom;


void main()
{
	switch(currentTex) {
		case TEXTURE_FINAL:
			fragColor = vec4(texture(tex_bloom, fs_in.texcoord).rgb, 1.0f);
			break;
		case TEXTURE_DIFFUSE:
			fragColor = vec4(texture(tex_diffuse, fs_in.texcoord).rgb, 1.0f);
			break;
		case TEXTURE_AMBIENT:
			fragColor = vec4(texture(tex_ambient, fs_in.texcoord).rgb, 1.0f);
			break;
		case TEXTURE_SPECULAR:
			fragColor = vec4(texture(tex_specular, fs_in.texcoord).rgb, 1.0f);
			break;
		case TEXTURE_WS_POSITION:
			vec3 ws_position = texelFetch(tex_ws_position, ivec2(gl_FragCoord.xy), 0).rgb;
			// fragColor = vec4(clamp(ws_position, vec3(0.0f), vec3(1.0f)), 1.0f);
			fragColor = vec4(ws_position, 1.0f);
			break;
		case TEXTURE_WS_NORMAL:
			vec3 ws_normal = texelFetch(tex_ws_normal, ivec2(gl_FragCoord.xy), 0).rgb;
			fragColor = vec4(clamp(ws_normal, vec3(0.0f), vec3(1.0f)), 1.0f);
			break;
		case TEXTURE_WS_TANGENT:
			vec3 ws_tangent = texelFetch(tex_ws_tangent, ivec2(gl_FragCoord.xy), 0).rgb;
			fragColor = vec4(clamp(ws_tangent, vec3(0.0f), vec3(1.0f)), 1.0f);
			break;
		case TEXTURE_PHONG:
			fragColor = vec4(texture(tex_phong, fs_in.texcoord).rgb, 1.0f);
			break;
		case TEXTURE_BLOOMHDR:
			fragColor = vec4(texture(tex_bloomHDR, fs_in.texcoord).rgb, 1.0f);
			break;
		case TEXTURE_PHONGSHADOW:
			fragColor = vec4(texture(tex_phongShadow, fs_in.texcoord).rgb, 1.0f);
			break;
		case TEXTURE_BLOOM:
			fragColor = vec4(texture(tex_bloom, fs_in.texcoord).rgb, 1.0f);
			break;
		default:
			fragColor = vec4(texture(tex_bloom, fs_in.texcoord).rgb, 1.0f);
			break;
	}
}