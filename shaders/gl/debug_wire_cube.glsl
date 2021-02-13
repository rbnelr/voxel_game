#version 460 core

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec4 col;
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec3 pos;
	layout(location = 1) in vec3 instance_pos;
	layout(location = 2) in vec3 instance_size;
	layout(location = 3) in vec4 instance_col;

	void main () {
		gl_Position = view.world_to_clip * vec4(pos * instance_size + instance_pos, 1);
		vs.col = instance_col;
	}
#endif

#ifdef _FRAGMENT
	layout(location = 0) out vec4 frag_col;
	void main () {
		frag_col = vs.col;
	}
#endif
