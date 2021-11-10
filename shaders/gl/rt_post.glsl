#version 460 core
#include "fullscreen_triangle.glsl"

#ifdef _FRAGMENT
	uniform sampler2D rt_col;
	uniform float exposure = 1.0;
	
	out vec3 frag_col;
	void main () {
		frag_col = texture(rt_col, vs_uv).rgb * exposure;
	}
#endif
