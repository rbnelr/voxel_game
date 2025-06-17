#version 460 core
#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec2 uv;
	vec3 pos_world;
	vec3 norm_world;
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec3 pos;
	layout(location = 1) in vec3 norm;
	layout(location = 2) in vec2 uv;

	uniform mat4x4 obj2world;
	
	void main () {
		vec4 pos_world = obj2world * vec4(pos, 1);
		gl_Position = view.world_to_clip * pos_world;
		vs.uv = uv;
		vs.pos_world = pos_world.xyz;
		vs.norm_world = mat3(obj2world) * norm;
	}
#endif

#ifdef _FRAGMENT
	uniform sampler3D cascade0_tex;
	
	uniform ivec3 world_base_pos;
	uniform ivec2 world_size;
	uniform float spacing;
	uniform ivec2 rays_oct;

	out vec4 frag_col;
	void main () {
		vec3 pos = vs.pos_world - vec3(world_base_pos);
		ivec3 probe_idx = ivec3(floor(pos / spacing));
		
	#if 0
		// cosine-weighted average light, aka diffuse light
		vec4 col = vec4(0.0);
		for (int y=0; y<rays_oct.y; y++)
		for (int x=0; x<rays_oct.x; x++) {
			vec2 uv = (vec2(ivec2(x,y)) + 0.5) / vec2(rays_oct);
			vec3 dir_world = octah2dir(uv);
			float cos_weight = max(dot(normalize(vs.norm_world), dir_world), 0.0);
			
			ivec3 P = ivec3(probe_idx.xy * rays_oct + ivec2(x,y), probe_idx.z);
			col += texelFetch(cascade0_tex, P, 0) * cos_weight;
		}
		frag_col = col / (rays_oct.x * rays_oct.y);
	#else
		vec3 view_vec = normalize(vs.pos_world - view.cam_pos);
		vec3 refl_dir = reflect(view_vec, normalize(vs.norm_world));
		
		//ivec2 uv = ivec2(dir2octah(refl_dir) * vec2(rays_oct));
		//ivec3 P = ivec3(probe_idx.xy * rays_oct + uv, probe_idx.z);
		//frag_col = texelFetch(cascade0_tex, P, 0);
		
		vec2 uv = dir2octah(refl_dir) * vec2(rays_oct);
		vec3 P = vec3(vec2(probe_idx.xy * rays_oct) + uv,
		             float(probe_idx.z) + 0.5);
		vec3 sz = vec3(textureSize(cascade0_tex, 0));
		frag_col = textureLod(cascade0_tex, P / sz, 0.0);
	#endif
	}
#endif
