#version 430 core

layout(location = 0) out vec4 fragColor; // Diffuse map
//layout(location = 1) out vec4 colorAmbient; // Ambient map
//layout(location = 2) out vec4 colorSpecular; // Specular map
//layout(location = 3) out vec4 colorVertex; // World Space Vertex map
//layout(location = 4) out vec4 colorNormal; // Normal map

in VS_OUT
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
} fs_in;

uniform bool ubPhongFlag;
uniform sampler2D tex;

uniform vec3 uv3Ambient;
uniform vec3 uv3Diffuse;
uniform vec3 uv3Specular;

vec3 texColor = texture(tex, fs_in.texcoord).rgb;

void blinnPhong() {
	vec3 ambient_albedo = texColor;
	vec3 diffuse_albedo = texColor;
	vec3 specular_albedo = vec3(1.0);
	float specular_power = 255.0;

	// Normalize the incoming N, L, V vectors
	vec3 N = normalize(fs_in.N);
	vec3 L = normalize(fs_in.L);
	vec3 V = normalize(fs_in.V);
	vec3 H = normalize(L + V);

	// Compute the ambient, diffuse and specular componts for each fragment
	vec3 ambient = ambient_albedo;
	vec3 diffuse = max(dot(N, L), 0.0) * diffuse_albedo;
	vec3 specular = pow(max(dot(N, H), 0.0), specular_power) * specular_albedo;

	vec3 Ia = vec3(0.1);
	vec3 Id = vec3(0.8);
	vec3 Is = vec3(0.1);

	fragColor = vec4(Ia * ambient + Id * diffuse + Is * specular, 1.0);
}

void main()
{
	if (ubPhongFlag) {
		blinnPhong();
	}
	else {
		fragColor = vec4(texColor, 1.0);
	}
}