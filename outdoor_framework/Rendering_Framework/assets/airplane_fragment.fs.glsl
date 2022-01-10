#version 410

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
	vec3 difftex = texture(tex, vertexData.texcoord).rgb;
	//vec3 ka = difftex;
	//vec3 kd = difftex;
	//vec3 ks = vec3(1);
	
	int n = 100;

	if (ubPhongFlag) {
		vec3 ka = vec3(0.1);
		vec3 kd = vec3(0.8);
		vec3 ks = vec3(0.1);
		vec3 Ia = uv3Ambient;
		vec3 Id = uv3Diffuse;
		vec3 Is = uv3Specular;
		//vec3 Ia = vec3(0.1);
		//vec3 Id = vec3(0.8);
		//vec3 Is = vec3(0.1);
		vec3 final_ambient = ka * Ia;
		vec3 final_diffuse = kd * Id * max(dot(vertexData.N, vertexData.L), 0.0);
		vec3 final_specular = ks * Is * pow(max(dot(vertexData.N, vertexData.H), 0.0), n);
		fragColor = vec4(final_ambient + final_diffuse + final_specular, 1.0);
		//fragColor = vec4(difftex, 1.0);
	}
	else {
		/*vec3 Ia = vec3(0.3);
		vec3 Id = vec3(0.3, 0.3, 0.9);
		vec3 Is = vec3(0.5, 0.5, 0.9);
		vec3 ambient = ka * Ia;
		vec3 diffuse = kd * Id * max(dot(vertexData.N, vertexData.L), 0.0);
		vec3 specular = ks * Is * pow(max(dot(vertexData.N, vertexData.H), 0.0), n);
		fragColor = vec4(ambient + diffuse + specular, 1.0);*/
		fragColor = vec4(difftex, 1.0);
	}
}