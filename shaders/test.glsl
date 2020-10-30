#version 450

#ifdef _VERTEX
	//layout(location = 0) in vec2 a_pos;
	//layout(location = 1) in vec3 a_col;
	#include "sub/inc.glsl"

	layout(location = 0) out vec3 vs_color;

	void main () {
		gl_Position = vec4(a_pos, 0.0, 1.0);
		vs_color = a_col;
	}
#endif

#ifdef _FRAGMENT
	layout(location = 0) in vec3 vs_color;
	layout(location = 0) out vec4 frag_color;

	void main () {
		frag_color = vec4(vs_color, 1.0);
	}
#endif
