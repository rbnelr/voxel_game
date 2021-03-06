#version 460 core

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3 uv;
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec2 pos;
	layout(location = 1) in vec3 uv;

	void main () {
		gl_Position = vec4(pos / view.viewport_size * 2.0 - 1.0, 0.0, 1.0);
		vs.uv = uv;
	}
#endif

#ifdef _FRAGMENT
	uniform sampler2D tex;
	uniform sampler2DArray tile_textures;

	out vec4 frag_col;
	void main () {
		if (vs.uv.z < 0)
			frag_col = texture(tex, vs.uv.xy);
		else
			frag_col = texture(tile_textures, vs.uv);
	}
#endif
