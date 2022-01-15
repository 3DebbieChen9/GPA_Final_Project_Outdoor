#version 430 core

layout(location = 0) out vec4 diffuse_color;
layout(location = 1) out vec4 ambient_color;
layout(location = 2) out vec4 specular_color;
layout(location = 3) out vec4 ws_position;
layout(location = 4) out vec4 normal;

in VS_OUT{
	vec3 N; // model space normal
	vec2 texcoord;
	vec3 FragPos;
	vec3 ps;
	vec3 nm;
	vec3 ambient_color;
	vec3 specular_color;
	vec3 diffuse_color; // diffuse color from mtl
} fs_in;

uniform sampler2D diffuseTexture;

//uniform samplerCube depthMap;

uniform vec3 light_pos;
uniform vec3 view_pos;

vec3 gen_diffuse_color() {
	/*vec3 objectColor;
	if (!tex_flag) {
		objectColor = fs_in.diffuse_color;
	}
	else {
		objectColor = texture(diffuseTexture, fs_in.texcoord).rgb;
	}
	return objectColor;*/
	return texture(diffuseTexture, fs_in.texcoord).rgb;
}

vec4 phone_shading() {

	vec3 lightPos = vec3(0.2, 0.6, 0.5);
	vec3 viewPos = view_pos;
	vec3 objectColor = gen_diffuse_color();


	float ambientStrength = 0.1;
	vec3 lightColor = vec3(1.0, 1.0, 1.0);
	vec3 ambient = ambientStrength * lightColor;

	// diffuse 
	vec3 norm = normalize(fs_in.N);
	vec3 lightDir = normalize(lightPos - fs_in.FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * lightColor;

	// specular
	float specularStrength = 0.5;
	vec3 viewDir = normalize(viewPos - fs_in.FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3 specular = specularStrength * spec * lightColor;

	vec3 result = (ambient + diffuse + specular) * objectColor;
	return vec4(result, 1.0);
}

vec3 gen_ambient() {
	float ambientStrength = 0.1;
	vec3 lightColor = vec3(1.0, 1.0, 1.0);
	return ambientStrength * lightColor;
}

vec3 gen_specular() {
	vec3 lightPos = vec3(-2.51449, 0.477241, -1.21263);
	vec3 viewPos = view_pos;
	vec3 lightDir = normalize(lightPos - fs_in.FragPos);
	vec3 lightColor = vec3(1.0, 1.0, 1.0);
	float specularStrength = 0.5;
	vec3 norm = normalize(fs_in.N);

	vec3 viewDir = normalize(viewPos - fs_in.FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	return specularStrength * spec * lightColor;
}

vec3 gen_diffuse() {
	vec3 lightPos = vec3(-2.51449, 0.477241, -1.21263);
	vec3 norm = normalize(fs_in.N);
	vec3 lightDir = normalize(lightPos - fs_in.FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 lightColor = vec3(1.0, 1.0, 1.0);

	return diff * lightColor;
}

vec3 gen_Bright() {
	vec3 c = texture(diffuseTexture, fs_in.texcoord).rgb;
	vec3 normal = normalize(fs_in.N);
	// ambient
	vec3 ambient = 0.0 * c;
	// lighting
	vec3 lighting = vec3(0.0);
	vec3 viewDir = normalize(view_pos - fs_in.FragPos);
	vec3 lightColor = vec3(1.0, 1.0, 1.0);

	vec3 light_position = light_pos;
	vec3 lightDir = normalize(light_position - fs_in.FragPos);
	float diff = max(dot(lightDir, normal), 0.0);
	vec3 result = lightColor * diff * c;
	// attenuation (use quadratic as we have gamma correction)
	float distance = length(fs_in.FragPos - light_position);
	result *= 1.0 / (distance * distance);
	lighting += result;

	result = ambient + lighting;
	float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
	vec4 BrightColor;
	//  if(brightness > 1.0)
	BrightColor = vec4(result, 1.0);
	// else
	  //   BrightColor = vec4(0.0, 0.0, 0.0, 1.0);

	return BrightColor.xyz;
}

void main()
{
	vec3 ambient = gen_ambient();
	vec3 color = gen_diffuse_color();
	vec3 specular = gen_specular();
	vec3 diffuse = gen_diffuse();

	dPosition = fs_in.ps;
	dNormal = fs_in.nm / 2 + 0.5;
	dAmbient = ambient;

	ddiffuse = diffuse;
	dSpecular = specular;
	
	dBloom = gen_Bright();
	dColor = gen_diffuse_color();
}