#version 460 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba16f, binding = 4) restrict uniform image2D out_tex;

#include "common.glsl"
#include "gpu_voxels.glsl"

uniform ivec3 world_base_pos;
uniform ivec2 world_size;
uniform int num_rays;
uniform float spacing;
uniform vec2 interval;
uniform ivec2 num_probes;
uniform ivec2 ray_regions;

uniform ivec2 dispatch_size;

uniform float higher_cascade_spacing;
uniform sampler2D higher_cascade;

const int scale_factor = 2;
const int branching_factor = 4;

vec4 sample_vox (vec2 local_pos) {
	ivec2 vox_pos = ivec2(floor(local_pos));
	ivec3 world_pos = world_base_pos + ivec3(vox_pos.x, 0, vox_pos.y);
	
	uint bid = read_voxel(world_pos);
	float texid = float(block_tiles[bid].sides[1]);
	
	vec4 col = bid <= B_AIR ? vec4(0) : textureLod(tile_textures, vec3(0.5,0.5, texid), 99.0).rgba;
	
	col.rgb *= get_emmisive(bid);
	
	return col;
}
bool out_of_bounds (vec2 local_pos) {
	ivec2 p = ivec2(floor(local_pos));
	return p.x < 0 || p.y < 0 || p.x >= world_size.x || p.y >= world_size.y;
}

vec4 avgerage_higher_cascade_rays (vec2 probe_pos, int ray_idx) {
	
	float higher_spacing = spacing * scale_factor;
	vec2 higher_probe_coord = probe_pos / higher_spacing - 0.5;
	
	int higher_rays = ray_idx * branching_factor;
	ivec2 higher_num_probes = num_probes / 2;
	ivec2 higher_ray_regions = ray_regions * 2;
	
	vec4 col = vec4(0);
	for (int ray=higher_rays; ray < higher_rays+branching_factor; ray++) {
		ivec2 ray_region = ivec2(ray % higher_ray_regions.x,
		                         ray / higher_ray_regions.y);
		vec2 uv = higher_probe_coord + ray_region * higher_num_probes;
		uv /= vec2(textureSize(higher_cascade, 0));
		col += texture(higher_cascade, uv, 0);
	}
	return col / float(branching_factor);
}

void main () {
	ivec2 invocIdx = ivec2(gl_GlobalInvocationID.xy);
	if (invocIdx.x >= dispatch_size.x || invocIdx.y >= dispatch_size.y)
		return;
		
	ivec2 _ray = invocIdx / num_probes;
	ivec2 probe_coord = invocIdx % num_probes;
	int ray_idx = _ray.x + ray_regions.x * _ray.y;
	
	// probes with spacing 1 are on voxel centers, spacing two on center of every second voxel
	vec2 probe_pos = spacing * (vec2(probe_coord) + 0.5);
	float angle_step = 2.0*PI / float(num_rays);
	float ang = (float(ray_idx) + 0.5) * angle_step;
	vec2 dir = vec2(cos(ang), sin(ang));
	
	float cur_dist = interval.x;
	
	vec4 col = vec4(0);
	for (int i=0; i<1000; i++) {
		if (cur_dist > interval.y) break;
		vec2 cur_pos = probe_pos + cur_dist * dir;
		if (out_of_bounds(cur_pos)) break;
		
		vec4 vox_col = sample_vox(cur_pos);
		col = vox_col;
		if (col.a > 0.9) break;
		
		cur_dist += min(spacing, 1);
	}
	
	vec4 total_col = col;
	
	if (higher_cascade_spacing > 0.0f && total_col.a < 0.999) {
		vec4 far_col = avgerage_higher_cascade_rays(probe_pos, ray_idx);
		total_col = mix(far_col, total_col, total_col.a);
	}
	
	imageStore(out_tex, invocIdx, total_col);
}
