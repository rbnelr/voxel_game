#version 460 core
#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec2 uv;
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec3 pos_world;
	layout(location = 2) in vec2 uv;

	uniform mat4x4 obj2world;
	
	void main () {
		gl_Position = view.world_to_clip * obj2world * vec4(pos_world, 1);
		vs.uv = uv;
	}
#endif

#ifdef _FRAGMENT
	uniform sampler2D tex;

	out vec4 frag_col;
	void main () {
		frag_col = texture(tex, vs.uv);
	}
#endif
