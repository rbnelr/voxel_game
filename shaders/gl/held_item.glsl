#version 460 core

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3 normal;
	vec3 uvi;
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec3 pos;
	layout(location = 1) in vec3 normal;
	layout(location = 2) in vec2 uv;

	uniform float texid;
	uniform mat4 model_to_world;

	void main () {
		gl_Position = view.world_to_clip * (model_to_world * vec4(pos, 1.0));

		vs.normal = mat3(model_to_world) * normal;
		vs.uvi = vec3(uv, texid);
	}
#endif

#ifdef _FRAGMENT
	uniform sampler2DArray tile_textures;

	const vec3 light_dir_world = normalize(vec3(1.33, 1.7, 5.9));
	vec3 basic_lighting (vec3 normal_world) {
		vec3 light = vec3(0.1, 0.15, 0.4) * 0.5;
		light += vec3(0.9, 0.9, 0.6) * max(dot(normal_world, light_dir_world), 0.0);
		return light;
	}

	out vec4 frag_col;
	void main () {
		vec4 col = texture(tile_textures, vs.uvi);

		col.rgb *= basic_lighting(normalize(vs.normal));

	#ifdef _WIREFRAME
		col = vec4(1,0,1,1);
	#endif
		frag_col = col;
	}
#endif
