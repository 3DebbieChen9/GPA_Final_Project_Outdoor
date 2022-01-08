#version 410

layout(location = 0) out vec4 fragColor;

uniform bool ubflag;
uniform sampler2D tex;

in VertexData
{
    vec3 N; // eye space normal
    vec3 L; // eye space light vector
    vec3 H; // eye space halfway vector
    vec2 texcoord;
} vertexData;

void main()
{
	// TODO: Modify fragment shader
	vec3 difftex = texture(tex, vertexData.texcoord).rgb;
	vec3 ka = difftex;
	vec3 kd = difftex;
	vec3 ks = vec3(1);
	int n = 100;

	if (ubflag == false) {
		vec3 Ia = vec3(0.3);
		vec3 Id = vec3(0.5);
		vec3 Is = vec3(1);
		vec3 ambient = ka * Ia;
		vec3 diffuse = kd * Id * max(dot(vertexData.N, vertexData.L), 0.0);
		vec3 specular = ks * Is * pow(max(dot(vertexData.N, vertexData.H), 0.0), n);
		fragColor = vec4(ambient + diffuse + specular, 1.0);
	}
	else {
		vec3 Ia = vec3(0.3);
		vec3 Id = vec3(0.3, 0.3, 0.9);
		vec3 Is = vec3(0.5, 0.5, 0.9);
		vec3 ambient = ka * Ia;
		vec3 diffuse = kd * Id * max(dot(vertexData.N, vertexData.L), 0.0);
		vec3 specular = ks * Is * pow(max(dot(vertexData.N, vertexData.H), 0.0), n);
		fragColor = vec4(ambient + diffuse + specular, 1.0);
	}
}