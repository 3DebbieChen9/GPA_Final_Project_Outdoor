#version 430 core

layout(location = 0) out vec4 fragColor;

in VS_OUT
{
	vec2 texcoord;
} fs_in;

uniform int DISPLAY;
uniform sampler2D tex_bloomHDR;
uniform sampler2D tex_phongColor;

uniform sampler2D tex_diffuse;
uniform sampler2D tex_ambient;
uniform sampler2D tex_specular;
uniform sampler2D tex_ws_position;
uniform sampler2D tex_ws_normal;
uniform sampler2D tex_ws_tangent;

void genBloom() {
	int half_size = 2;
	vec4 color_sum = vec4(0);

	for (int i = -half_size; i <= half_size; ++i) {
		for (int j = -half_size; j <= half_size; ++j) {
			ivec2 coord = ivec2(gl_FragCoord.xy) + ivec2(i, j);
			color_sum += texelFetch(tex_bloomHDR, coord, 0);
		}
	}
	int sample_count = (half_size * 2 + 1) * (half_size * 2 + 1);
	vec4 color = color_sum / sample_count;
	vec3 originalColor = texture(tex_phongColor, fs_in.texcoord).rgb;
	fragColor = vec4((originalColor * vec3(0.2) + color.xyz * vec3(0.8)), 1.0f);
}

void main()
{
	switch(DISPLAY) {
		case 1:
			fragColor = vec4(texture(tex_diffuse, fs_in.texcoord).rgb, 1.0f);
			break;
		case 2: 
			fragColor = vec4(texture(tex_ambient, fs_in.texcoord).rgb, 1.0f);
			break;
		case 3: 
			fragColor = vec4(texture(tex_specular, fs_in.texcoord).rgb, 1.0f);
			break;
		case 4: 
			fragColor = vec4(texture(tex_ws_position, fs_in.texcoord).rgb, 1.0f);
			break;
		case 5: 
			fragColor = vec4(texture(tex_ws_normal, fs_in.texcoord).rgb, 1.0f);
			break;
		case 6: 
			fragColor = vec4(texture(tex_phongColor, fs_in.texcoord).rgb, 1.0f);
			break;
		case 7: 
			genBloom();
			break;	
		default:
			fragColor = vec4(texture(tex_phongColor, fs_in.texcoord).rgb, 1.0f);
			break;
	}
}