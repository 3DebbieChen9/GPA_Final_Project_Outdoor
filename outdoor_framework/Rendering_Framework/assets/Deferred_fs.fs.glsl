#version 430 core

layout(location = 0) out vec4 phongColor; // Phong-Shading Result
layout(location = 1) out vec4 bloomHDR;

in VS_OUT
{
	vec2 texcoord;
} fs_in;

uniform vec3 uv3LightPos;
uniform vec3 uv3eyePos;

uniform sampler2D tex_diffuse;
uniform sampler2D tex_ambient;
uniform sampler2D tex_specular;
uniform sampler2D tex_ws_position;
uniform sampler2D tex_ws_normal;
uniform sampler2D tex_ws_tangent;

vec3 phongShading() {

	// Eye space to tangent space TBN
	vec3 vs_T = normalize(texelFetch(tex_ws_tangent, ivec2(gl_FragCoord.xy), 0).xyz);
	vec3 vs_N = normalize(texelFetch(tex_ws_normal, ivec2(gl_FragCoord.xy), 0).xyz * 2.0 - vec3(1.0)); // out
	vec3 vs_B = cross(vs_N, vs_T);

	vec4 vs_P = vec4(texelFetch(tex_ws_position, ivec2(gl_FragCoord.xy), 0).xyz, 1.0);
	
	// Parallel Light
	vec3 vs_L = -uv3LightPos;// - vs_P.xyz;
	vec3 lightDir = normalize(vec3(dot(vs_L, vs_T), dot(vs_L, vs_B), dot(vs_L, vs_N))); /// out
	vec3 vs_V = uv3eyePos - vs_P.xyz;
	vec3 eyeDir = normalize(vec3(dot(vs_V, vs_T), dot(vs_V, vs_B), dot(vs_V, vs_N))); /// out
	
	// vec3 fs_V = normalize(eyeDir);
	// vec3 fs_L = normalize(lightDir);
	// vec3 fs_N = normalize(vs_N);
	vec3 fs_V = eyeDir;
	vec3 fs_L = lightDir;
	vec3 fs_N = vs_N;

	vec3 fs_R = reflect(-fs_L, fs_N);

	vec3 diffuse_albedo = vec3(10.0) * texture(tex_diffuse, fs_in.texcoord).rgb;
	vec3 diffuse = max(dot(fs_N, fs_L), 0.0) * diffuse_albedo;

	vec3 specular_albedo = texture(tex_specular, fs_in.texcoord).rgb;
	vec3 specular = max(pow(dot(fs_R, fs_V), 900.0), 0.0) * specular_albedo;

	vec3 ambient = texture(tex_ambient, fs_in.texcoord).rgb;

	vec3 color = ambient * vec3(0.1) + diffuse * vec3(0.8) + specular * vec3(0.1);

	// Point Light
	vec3 pointLightPos = vec3(636.48, 134.79, 495.98);
	vec3 point_L = pointLightPos - vs_P.xyz;
	lightDir = normalize(vec3(dot(point_L, vs_T), dot(point_L, vs_B), dot(point_L, vs_N)));
	fs_L = normalize(lightDir);
	fs_R = reflect(-fs_L, fs_N);

	diffuse = max(dot(fs_N, fs_L), 0.0) * diffuse_albedo;
	specular = max(pow(dot(fs_R, fs_V), 900.0), 0.0) * specular_albedo;

	float distance = length(pointLightPos - vs_P.xyz);
	float attenuation = 50.0f / (pow(distance, 2) + 0.5);
	color += ((diffuse * vec3(0.8) + specular * vec3(0.2)) * attenuation);

	// phongColor = vec4(color, 1.0);
	return color;
}

void genBloomHDR(vec3 _color) {
	
	float brightness = dot(_color, vec3(0.8));
	bloomHDR = vec4(0.0f);
	if (brightness > 1) {
		bloomHDR = vec4(_color, 1.0);
	}
	else {
		bloomHDR = vec4(vec3(0.0f), 1.0);
	}

}

void main()
{
	vec3 defferdColor = phongShading();
	genBloomHDR(defferdColor);
	phongColor = vec4(defferdColor, 1.0f);
}