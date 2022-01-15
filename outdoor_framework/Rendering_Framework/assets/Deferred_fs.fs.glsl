#version 410 core

layout(location = 0) out vec4 phongColor; // Phong-Shading Result
layout(location = 1) out vec4 bloomHDR;

in VS_OUT
{
	vec2 texcoord;
} fs_in;

uniform vec3 uv3LightPos;
uniform sampler2D tex_ws_position;
uniform sampler2D tex_ws_normal;
uniform sampler2D tex_ws_tangent;
uniform sampler2D tex_diffuse;
uniform sampler2D tex_ambient;
uniform sampler2D tex_specular;

vec3 afterPhongColor = vec3(1.0f);
void phongShading() {

	vec4 vs_P = vec4(texelFetch(tex_ws_position, ivec2(gl_FragCoord.xy), 0).xyz, 1.0);
	// Eye space to tangent space TBN
	vec3 vs_T = normalize(texelFetch(tex_ws_tangent, ivec2(gl_FragCoord.xy), 0).xyz);
	vec3 vs_N = normalize(texelFetch(tex_ws_normal, ivec2(gl_FragCoord.xy), 0).xyz * 2.0 - vec3(1.0f));
	vec3 vs_B = cross(vs_N, vs_T);

	// ¥­¦æ¥ú
	vec3 vs_L = -uv3LightPos;// - vs_P.xyz;
	vec3 lightDir = normalize(vec3(dot(vs_L, vs_T), dot(vs_L, vs_B), dot(vs_L, vs_N)));
	vec3 vs_V = -vs_P.xyz;
	vec3 eyeDir = normalize(vec3(dot(vs_V, vs_T), dot(vs_V, vs_B), dot(vs_V, vs_N)));
	
	vec3 fs_V = normalize(eyeDir);
	vec3 fs_L = normalize(lightDir);
	vec3 fs_N = normalize(vs_N);

	vec3 fs_R = reflect(-fs_L, fs_N);

	vec3 diffuse_albedo = vec3(3.0) * texture(tex_diffuse, fs_in.texcoord).rgb;
	vec3 diffuse = max(dot(fs_N, fs_L), 0.0) * diffuse_albedo;

	vec3 specular_albedo = texture(tex_specular, fs_in.texcoord).rgb;
	vec3 specular = max(pow(dot(fs_R, fs_V), 20.0), 0.0) * specular_albedo;

	vec3 ambient = texture(tex_ambient, fs_in.texcoord).rgb;
	afterPhongColor = ambient * vec3(0.1) + diffuse * vec3(0.8) + specular * vec3(0.1);
	phongColor = vec4(afterPhongColor, 1.0);
}

void genBloomHDR() {
	//vec2 img_size = vec2(2.0f, 2.0f);
	//vec4 color_sum = vec4(0);
	//// Blur origin image
	//int n = 0;
	//int half_size = 10;
	//for (int i = -half_size; i <= half_size; ++i) {
	//	for (int j = -half_size; j <= half_size; ++j) {
	//		ivec2 coord = ivec2(gl_FragCoord.xy) + ivec2(i, j);
	//		color_sum += texelFetch(tex_diffuse, coord, 0);
	//	}
	//}
	//int sample_count = (half_size * 2 + 1) * (half_size * 2 + 1);
	//bloomHDR = color_sum / sample_count;

}

void main()
{
	phongShading();
	genBloomHDR();
}