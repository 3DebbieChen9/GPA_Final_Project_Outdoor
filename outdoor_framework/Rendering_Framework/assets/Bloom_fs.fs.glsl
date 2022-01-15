#version 430 core

layout(location = 0) out vec4 fragColor;

in VS_OUT
{
	vec2 texcoord;
} fs_in;

uniform sampler2D tex_phongColor;
uniform sampler2D tex_bloomHDR;

void main()
{
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