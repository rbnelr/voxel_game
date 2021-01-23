#version 460

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec4 col;
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec3 pos_world;
	layout(location = 1) in vec4 color;

	void main () {
		gl_Position = view.world_to_clip * vec4(pos_world, 1);
		vs.col = color;
	}
#endif

#ifdef _FRAGMENT
	layout(location = 0) out vec4 frag_col;
	void main () {
		frag_col = vs.col;
	}
#endif
