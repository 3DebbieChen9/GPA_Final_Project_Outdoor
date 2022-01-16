#version 430 core

layout(location = 1) in vec3 fv3specular;
layout(location = 2) in vec3 fv3ambient;
layout(location = 3) in vec3 fv3diffuse;
layout(location = 4) in vec3 iv3vertex;
layout(location = 5) in vec3 iv3normal;
layout(location = 6) in vec2 iv2tex_coord;
layout(location = 7) in vec3 tangents;
layout(location = 8) in vec3 bitangents;

uniform mat4 um4m;
uniform mat4 um4mv;
uniform mat4 um4p;

out VertexData
{
    vec3 N; // model space normal
    vec2 texcoord;
    vec3 FragPos;

    vec3 ambient_color;
    vec3 specular_color;
    vec3 diffuse_color; // diffuse color from mtl

    vec3 tangent;
	mat3 TBN;
} vertexData;
out vec4 FragPosLightSpace;

uniform mat4 light_vp;

void main()
{
	gl_Position = um4p * um4mv * vec4(iv3vertex, 1.0);

    // non reverse
    // vertexData.N = transpose(inverse(mat3(um4m))) * iv3normal;
    vertexData.N = iv3normal;
    vertexData.FragPos = vec3(um4m * vec4(iv3vertex, 1.0));
    vertexData.texcoord = iv2tex_coord;
    vertexData.diffuse_color = fv3diffuse;
    vertexData.specular_color = fv3specular;
    vertexData.ambient_color = fv3ambient;

    // frag position in light space
    FragPosLightSpace = light_vp * vec4(vertexData.FragPos, 1.0);

	vertexData.tangent = tangents;
	vec3 T = normalize(vec3(um4m * vec4(tangents,   0.0)));
	vec3 N = normalize(vec3(um4m * vec4(iv3normal,  0.0)));
	vec3 B = normalize(vec3(um4m * vec4(bitangents, 0.0)));
	mat3 TBN = mat3(T,B,N);
	vertexData.TBN = TBN;
}