#version 430 core

layout(location = 0) out vec4 fragColor;

in VS_OUT
{
	vec3 N; // eye space normal
	vec3 L; // eye space light vector
	vec3 V; // eye space view vector
	vec2 texcoord;
	vec3 eyeDir;
	vec3 lightDir;
	//vec3 normal;
} fs_in;

//uniform bool ubPhongFlag;
//uniform bool ubNormalFlag;

uniform sampler2D tex_diffuse;
uniform sampler2D tex_normal;

uniform vec3 uv3Ambient;
uniform vec3 uv3Diffuse;
uniform vec3 uv3Specular;

vec3 texColor = texture(tex_diffuse, fs_in.texcoord).rgb;

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

void normalMapping() {
	vec3 V = normalize(fs_in.eyeDir);
	vec3 L = normalize(fs_in.lightDir);
	vec3 N = normalize(texture(tex_normal, fs_in.texcoord).rgb * 2.0 - vec3(1.0));

	// Calculate R ready for use in Phong lighting.
	vec3 R = reflect(-L, N); 

	//vec3 ambient_albedo = texColor;
	vec3 ambient = texColor;

	vec3 diffuse_albedo = texColor;
	vec3 diffuse = max(dot(N, L), 0.0) * diffuse_albedo;
	
	vec3 specular_albedo = vec3(1.0);
	vec3 specular = max(pow(dot(R, V), 20.0), 0.0) * specular_albedo;

	fragColor = vec4(diffuse + specular, 1.0);
	//fragColor = vec4(texture(texNormal, fs_in.texcoord).rgb, 1.0);
}

void main()
{
	/*if (ubPhongFlag) {
		blinnPhong();
	}
	else if (ubNormalFlag) {
		normalMapping();
	}
	else {
		fragColor = vec4(texColor, 1.0);
	}*/
	//fragColor = vec4(texColor, 1.0);
	normalMapping();
}