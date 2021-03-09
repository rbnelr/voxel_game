#version 460 core

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3 normal;
	vec3 uvi; // uv + tex_index
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec2 pos;
	layout(location = 1) in vec3 normal;
	layout(location = 2) in vec3 uvi;

	void main () {
		gl_Position = vec4(pos / view.viewport_size * 2.0 - 1.0, 0.0, 1.0);
		vs.normal = normal;
		vs.uvi = uvi;
	}
#endif

#ifdef _FRAGMENT
	uniform sampler2D tex;
	uniform sampler2DArray tile_textures;

	const vec3 light_dir = normalize(vec3(0.33, 1.7, 5.9));

	out vec4 frag_col;
	void main () {
		vec4 col;
		if (vs.uvi.z < 0) {
			col = texture(tex, vs.uvi.xy);
		} else {
			col = texture(tile_textures, vs.uvi);
			col.rgb *= max(dot(normalize(vs.normal), normalize(light_dir)), 0.0);
		}

	#ifdef _WIREFRAME
		col = vec4(0,0,1,1);
	#endif
		frag_col = col;
	}
#endif
