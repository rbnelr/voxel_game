#version 460 core

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3 uvi;
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec3 pos;
	//layout(location = 1) in vec3 normal;
	layout(location = 2) in vec2 uv;

	uniform vec3 pos_world;
	uniform float texid;

	void main () {
		gl_Position = view.world_to_clip * vec4(pos * 1.001 - 0.0005 + pos_world, 1.0);

		vs.uvi = vec3(uv, texid);
	}
#endif

#ifdef _FRAGMENT
	uniform sampler2DArray tile_textures;

	out vec4 frag_col;
	void main () {
		vec4 col = texture(tile_textures, vs.uvi);

	#ifdef _WIREFRAME
		col = vec4(0,1,0,1);
	#endif
		frag_col = col;
	}
#endif
