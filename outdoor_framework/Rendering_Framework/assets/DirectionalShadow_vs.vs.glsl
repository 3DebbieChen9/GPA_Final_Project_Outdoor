#version 430 core

layout (location = 0) in vec3 iv3vertex;

uniform mat4 light_vp;
uniform mat4 um4m;

void main()
{
    gl_Position = light_vp * um4m * vec4(iv3vertex, 1.0); // light space
}