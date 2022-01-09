#version 410

out vec4 fragColor;

in VS_OUT
{
	vec2 texcoord;
} fs_in;

uniform sampler2D tex;

void main() {
	vec3 texColor = texture(tex, fs_in.texcoord).rgb;
	fragColor = vec4(texColor, 1.0);
}