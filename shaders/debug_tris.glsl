#version 460

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3 pos_cam;
	vec3 normal_cam;
	vec4 col;
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec3 pos_world;
	layout(location = 1) in vec3 normal;
	layout(location = 2) in vec4 color;

	void main () {
		gl_Position = world_to_clip * vec4(pos_world, 1);
		vs.pos_cam = (world_to_cam * vec4(pos_world, 1)).xyz;
		vs.normal_cam = mat3(world_to_cam) * normal;
		vs.col = color;
	}
#endif

#ifdef _FRAGMENT
	const vec3 light_pos_cam = vec3(0.1, 0.2, 0.8);
	const float light_col = 0.6;
	const float ambient_col = 0.4;

	void main () {
		vec3 light_dir_cam = normalize(light_pos_cam - vs.pos_cam);

		vec4 col = vs.col;
		col.rgb *= max(dot(normalize(vs.normal_cam), light_dir_cam), 0.0) * light_col + ambient_col;
		frag_col = col;
	}
#endif
