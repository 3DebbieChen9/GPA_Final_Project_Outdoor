#version 430 core

layout(location = 0) out vec4 fragColor;

in VS_OUT
{
	vec2 texcoord;
} fs_in;

uniform sampler2D tex;

void main() {
	vec3 texColor = texture(tex, fs_in.texcoord).rgb;
	fragColor = vec4(vec3(0.0, 0.0, 1.0), 1.0);
}