#version 410

layout(location = 0) in vec3 iv3vertex;
layout(location = 1) in vec2 iv2tex_coord;
layout(location = 2) in vec3 iv3normal;

uniform mat4 um4mv;
uniform mat4 um4p;

out VertexData
{
    vec3 N; // eye space normal
    vec3 L; // eye space light vector
    vec3 H; // eye space halfway vector
    vec2 texcoord;
} vertexData;

void main()
{
	gl_Position = um4p * um4mv * vec4(iv3vertex, 1.0);
	vertexData.texcoord = iv2tex_coord;

	vec4 P = um4mv * vec4(iv3vertex, 1.0);
	vec3 V = normalize(-P.xyz);
	vec3 light_pos = vec3(0.2, 0.6, 0.5);

	vertexData.N = normalize(mat3(um4mv) * iv3normal);
	vertexData.L = normalize(light_pos - P.xyz);
	vertexData.H = normalize(vertexData.L + V);
}