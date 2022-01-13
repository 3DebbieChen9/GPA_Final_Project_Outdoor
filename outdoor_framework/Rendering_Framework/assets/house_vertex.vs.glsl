#version 430 core

layout(location = 0) in vec3 iv3vertex;
layout(location = 1) in vec2 iv2tex_coord;
layout(location = 2) in vec3 iv3normal;

uniform mat4 um4m;
uniform mat4 um4v;
uniform mat4 um4p;
uniform mat4 um4Lightmvp;

out VertexData
{
    vec3 N; // eye space normal
    vec3 L; // eye space light vector
    vec3 H; // eye space halfway vector
	vec3 normal;
    vec2 texcoord;
	vec4 lightCoord;
} vertexData;

void main()
{
	gl_Position = um4p * um4v * um4m * vec4(iv3vertex, 1.0);
	vertexData.texcoord = iv2tex_coord;
	vertexData.normal = iv3normal;
	vertexData.lightCoord = um4Lightmvp * vec4(iv3vertex, 1.0);

	vec4 P = um4v * um4m * vec4(iv3vertex, 1.0);
	vec3 V = normalize(-P.xyz);
	vec3 light_pos = vec3(0.2, 0.6, 0.5);

	vertexData.N = normalize(mat3(um4v * um4m) * iv3normal);
	vertexData.L = normalize(light_pos - P.xyz);
	vertexData.H = normalize(vertexData.L + V);	
}