#version 460 core
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(rgba16f, binding = 4) restrict uniform image3D out_tex;

#include "common.glsl"
#include "gpu_voxels.glsl"

uniform ivec3 dispatch_size;

uniform ivec3 world_base_pos;
uniform ivec2 world_size;
uniform int num_rays;
uniform float spacing;
uniform vec2 interval;
uniform float scale_factor;
uniform float branching_factor;

uniform bool has_higher_cascade;
uniform sampler3D higher_cascade;

// Expensive! This exact stuff is already cached by VCT texture!
vec4 voxel_col_lookup (ivec3 world_pos) {
	uint bid = read_voxel(world_pos);
	float texid = float(block_tiles[bid].sides[1]);
	vec4 col = bid <= B_AIR ? vec4(0) : textureLod(tile_textures, vec3(0.5,0.5, texid), 99.0).rgba;
	col.rgb *= get_emmisive(bid);
	return col;
}
vec4 sample_scene (vec2 local_pos) {
#if 1
	ivec2 vox_pos = ivec2(floor(local_pos));
	ivec3 world_pos = world_base_pos + ivec3(vox_pos.x, 0, vox_pos.y);
	
	uint bid = read_voxel(world_pos);
	float texid = float(block_tiles[bid].sides[1]);
	
	vec4 col = bid <= B_AIR ? vec4(0) : textureLod(tile_textures, vec3(0.5,0.5, texid), 99.0).rgba;
	
	col.rgb *= get_emmisive(bid);
	
	return col;
#else
	// Bilinear voxel emiss + alpha lookup
	// I thought this would improve artefacting, but it causes rays starting close to a voxel and going away from it to still get some alpha
	// This (at least partially) causes even more artefacting
	vec2 _floored = floor(local_pos - 0.5);
	vec2 t = local_pos - 0.5 - _floored;
	
	ivec3 world_pos = world_base_pos + ivec3(int(_floored.x), 0, int(_floored.y));
	
	vec4 col00 = voxel_col_lookup(world_pos);
	vec4 col01 = voxel_col_lookup(world_pos + ivec3(1, 0, 0));
	vec4 col10 = voxel_col_lookup(world_pos + ivec3(0, 0, 1));
	vec4 col11 = voxel_col_lookup(world_pos + ivec3(1, 0, 1));
	
	return mix(mix(col00, col01, t.x), mix(col10, col11, t.x), t.y);
#endif
}
bool out_of_bounds (vec2 local_pos) {
	ivec2 p = ivec2(floor(local_pos));
	return p.x < 0 || p.y < 0 || p.x >= world_size.x || p.y >= world_size.y;
}

vec4 avgerage_higher_cascade_rays (vec2 probe_pos, int ray_idx) {
	
	float hi_spacing = spacing * scale_factor;
	vec2 hi_probe_coord = probe_pos / hi_spacing - 0.5;
	float hi_rays = float(ray_idx) * branching_factor;
	
	vec3 texsz = 1.0 / textureSize(higher_cascade, 0);
	
	// While it is kind of obvious in this version, I don't pre-average
	// since my understanding is that if you store all rays, you can later evaluate hemispheres for 3d surface lighting or even attempt to extract specular light
	vec4 col = vec4(0);
	for (float ray=hi_rays; ray < hi_rays + branching_factor; ray++) {
		vec3 uv = (vec3(hi_probe_coord, ray) + 0.5) * texsz;
		col += texture(higher_cascade, uv, 0);
	}
	return col / branching_factor;
}

void main () {
	ivec3 invocID = ivec3(gl_GlobalInvocationID);
	if (any(greaterThan(invocID, dispatch_size))) return;
	
	ivec2 probe_coord = invocID.xy;
	int ray_idx = invocID.z;
	
	// spacing 1: 0.5, 1.5, 2.5, so voxel centers
	// spacing 2: 1.0, 3.0, 5.0, so between voxels
	vec2 probe_pos = spacing * (vec2(probe_coord) + 0.5);
	// counter-clockwise rays, first cascade X pattern, next subdivided like standard layout
	float angle_step = 2.0*PI / float(num_rays);
	float ang = (float(ray_idx) + 0.5) * angle_step;
	vec2 dir = vec2(cos(ang), sin(ang));
	
	// start exactly where previous raymarching left off
	// fudge a little, try to combat halo
	float cur_dist = max(interval.x - spacing*0.25, 0.0);
	
	vec4 col = vec4(0);
	for (int i=0; i<1000; i++) {
		if (cur_dist > interval.y) break;
		vec2 cur_pos = probe_pos + cur_dist * dir;
		if (out_of_bounds(cur_pos)) break;
		
		vec4 vox_col = sample_scene(cur_pos);
		if (vox_col.a > 0.5) {
			col = vec4(vox_col.rgb, 1.0);
			break; // arbitrary cutoff, note that colors are raw voxel texture alphas, no interpolation
		}
		
		cur_dist += min(spacing, 0.5); // helps with aliasing
	}
	
	if (has_higher_cascade && col.a < 0.999) {
		vec4 far_col = avgerage_higher_cascade_rays(probe_pos, ray_idx);
		col = mix(far_col, col, col.a);
	}
	
	imageStore(out_tex, invocID, col);
}
