#version 430 core

layout(location = 0) out vec4 fragColor;

in VertexData
{
    vec3 N; // eye space normal
    vec3 L; // eye space light vector
    vec3 H; // eye space halfway vector
	vec3 normal;
    vec2 texcoord;
	vec4 lightCoord;
} vertexData;

uniform bool ubPhongFlag;
uniform sampler2D tex;

uniform vec3 uv3Ambient;
uniform vec3 uv3Diffuse;
uniform vec3 uv3Specular;

void main()
{
	vec3 texColor = texture(tex, vertexData.texcoord).rgb;
	if (ubPhongFlag) {
		vec3 ka = texColor;
		vec3 kd = texColor;
		vec3 ks = vec3(1.0);
		int n = 100;

		vec3 Ia = vec3(0.1);
		vec3 Id = vec3(0.8);
		vec3 Is = vec3(0.1);
		vec3 ambient = ka * Ia;
		vec3 diffuse = kd * Id * max(dot(vertexData.N, vertexData.L), 0.0);
		vec3 specular = ks * Is * pow(max(dot(vertexData.N, vertexData.H), 0.0), n);
		fragColor = vec4(ambient + diffuse + specular, 1.0);
	}
	else {
		fragColor = vec4(texColor, 1.0);
	}
}