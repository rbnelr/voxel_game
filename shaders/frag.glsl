#version 450

layout(location = 0) in vec3 vs_color;
layout(location = 0) out vec4 frag_color;

void main () {
	frag_color = vec4(vs_color, 1.0);
}
