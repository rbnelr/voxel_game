#version 440 core
#include "common.glsl"

#ifdef _VERTEX
const vec2 poss[6] = { vec2(1,0), vec2(1,1), vec2(0,0), vec2(0,0), vec2(1,1), vec2(0,1) };
void main () {
	int instance = gl_VertexID / 6;
	int vertex = gl_VertexID % 6;

	vec2 offs = vec2(instance % 32, instance / 32) * 3;
	gl_Position = view.world_to_clip * vec4(poss[vertex] + offs, 0, 1);
}
#endif

#ifdef _FRAGMENT
layout(location = 0) out vec4 frag_col;
void main () {
	frag_col = vec4(0.0, 0.0, 1.0, 1.0);
}
#endif
