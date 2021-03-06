#version 460 core

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec4	col;
} vs;

#ifdef _VERTEX
	layout (location = 0) in vec3 pos;
	//layout (location = 1) in vec3 norm;
	//layout (location = 2) in vec2 uv;
	//layout (location = 3) in vec4 col;

	uniform	vec3	block_pos;
	uniform	mat3	face_rotation;
	uniform	vec4	tint;

	void main () {
		gl_Position =		view.world_to_clip * vec4(face_rotation * pos + block_pos + vec3(0.5), 1);
		vs.col =			tint;
	}
#endif

#ifdef _FRAGMENT
	out vec4 frag_col;

	void main () {
		frag_col = vs.col;
	}
#endif
