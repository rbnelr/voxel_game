#version 460 core
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(rgba16f, binding = 4) restrict uniform image3D out_tex;

#define DEBUGDRAW 1

#include "common.glsl"

#if DEBUGDRAW
#include "dbg_indirect_draw.glsl"

uniform vec4 dbg_col;
uniform vec2 dbg_pos;
uniform int dbg_ray;
#endif

#include "gpu_voxels.glsl"


uniform ivec3 dispatch_size;

uniform int cascade;
uniform ivec3 world_base_pos;
uniform ivec2 world_size;
uniform int num_rays;
uniform float spacing;
uniform vec2 interval;
uniform vec2 hi_interval;
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
	// Exact voxel raymarching
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

// rgb: linear light emitted along ray
// a: opacity blocking light along ray
vec4 blend_light (vec4 background, vec4 foreground) {
	return mix(background, vec4(foreground.rgb, 1.0), foreground.aaaa);
}

vec4 _dbg_col;

vec4 trace_ray (vec2 point, vec2 dir, float start_dist, float max_dist) {
#if DEBUGDRAW
	if (update_debugdraw) {
		ivec2 probe_coord = ivec3(gl_GlobalInvocationID).xy;
		int ray_idx = ivec3(gl_GlobalInvocationID).z;
		
		ivec2 coord = ivec2(round(dbg_pos / spacing - 0.5));
		ivec2 diff = (coord - probe_coord); // abs
		
		if (  cascade <= 1 &&
			(cascade == 0 ? ray_idx == dbg_ray : ray_idx/4 == dbg_ray/4) &&
			  diff.x >= 0 && diff.x <= 0 &&
			  diff.y >= 0 && diff.y <= 0  ) {
			vec2 a = point + start_dist * dir;
			vec2 ab = (max_dist - start_dist) * dir;
			
			dbgdraw_vector(world_base_pos + vec3(a.x, .95, a.y), vec3(ab.x, 0, ab.y), _dbg_col);
			
			//if (c.a > 0.1) {
			//	dbgdraw_point(world_base_pos + vec3(probe_pos.x, .95, probe_pos.y), 0.03, dbg_col);
			//}
		}
	}
#endif
	
	float cur_dist = start_dist;
	
	vec4 col = vec4(0);
	for (int i=0; i<1000; i++) {
		if (cur_dist > max_dist) break;
		vec2 cur_pos = point + cur_dist * dir;
		if (out_of_bounds(cur_pos)) break;
		
		vec4 vox_col = sample_scene(cur_pos);
		if (vox_col.a > 0.5) {
			col = vec4(vox_col.rgb, 1.0);
			break; // arbitrary cutoff, note that colors are raw voxel texture alphas, no interpolation
		}
		
		cur_dist += min(spacing, 0.5); // raymarch at half voxel res, seems to help with aliasing
	}
	return col;
}
vec4 trace_ray_between (vec2 start, vec2 end) {
	vec2 dir = end - start;
	float max_dist = length(dir);
	dir = normalize(dir);
	float cur_dist = 0.0;
	
	return trace_ray(start, dir, 0.0, max_dist);
}

vec4 avgerage_higher_cascade_rays (vec2 probe_pos, int ray_idx, vec2 ray_end, vec2 ray_dir) {
	
#if 0
	// Hardware bilinear filtering without bilinear fix
	float hi_spacing = spacing * scale_factor;
	vec2 hi_probe_coord = probe_pos / hi_spacing - 0.5;
	float hi_rays = float(ray_idx) * branching_factor;
	
	vec3 texsz = 1.0 / textureSize(higher_cascade, 0);
	
	// While it is kind of obvious in this version, I don't pre-average
	// since my understanding is that if you store all rays, you can later evaluate hemispheres for 3d surface lighting or even attempt to extract specular light
	vec4 col = vec4(0);
	for (float ray=hi_rays; ray < hi_rays + branching_factor; ray++) {
		vec3 uv = (vec3(hi_probe_coord, ray) + 0.5) * texsz;
		col += texture(higher_cascade, uv);
	}
	return col / branching_factor;
#else
	// Bilinear fix implementation
	
	// parent cascade probe spacing and ray count
	float hi_spacing = spacing * scale_factor;
	int hi_rays = ray_idx * int(branching_factor);
	// parent probe indices and bilinear factors based on this probe's position
	vec2 hi_probe_coordF = probe_pos / hi_spacing - 0.5;
	vec2 tmpF = floor(hi_probe_coordF);
	ivec2 hi_probe_coord = ivec2(tmpF);
	vec2 bilin = hi_probe_coordF - tmpF;
	
	//
	vec2 hi_probe_pos = hi_spacing * (tmpF + 0.5);
	vec2 hi_rays_start = hi_probe_pos + hi_interval.x * ray_dir;
	
	_dbg_col = vec4(_dbg_col.rgb, 0.1);
	vec4 fix00 = trace_ray_between(ray_end, hi_rays_start );
	vec4 fix10 = trace_ray_between(ray_end, hi_rays_start + vec2(hi_spacing, 0.0));
	vec4 fix01 = trace_ray_between(ray_end, hi_rays_start + vec2(0.0, hi_spacing));
	vec4 fix11 = trace_ray_between(ray_end, hi_rays_start + vec2(hi_spacing, hi_spacing));
	
	// Manual bilinear sample with no pre-averaging
	vec4 c00 = vec4(0);
	vec4 c10 = vec4(0);
	vec4 c01 = vec4(0);
	vec4 c11 = vec4(0);
	for (int ray=hi_rays; ray < hi_rays + int(branching_factor); ray++) {
		ivec3 P = ivec3(hi_probe_coord, ray);
		c00 += texelFetchOffset(higher_cascade, P, 0, ivec3(0,0,0));
		c10 += texelFetchOffset(higher_cascade, P, 0, ivec3(1,0,0));
		c01 += texelFetchOffset(higher_cascade, P, 0, ivec3(0,1,0));
		c11 += texelFetchOffset(higher_cascade, P, 0, ivec3(1,1,0));
	}
	//c00 = blend_light(c00, fix00);
	//c10 = blend_light(c10, fix10);
	//c01 = blend_light(c01, fix01);
	//c11 = blend_light(c11, fix11);
	c00 = c00 * (1.0 - fix00.a);
	c10 = c10 * (1.0 - fix10.a);
	c01 = c01 * (1.0 - fix01.a);
	c11 = c11 * (1.0 - fix11.a);
	
	vec4 col = mix(mix(c00, c10, bilin.x),
				   mix(c01, c11, bilin.x), bilin.y);
	return col / branching_factor;
#endif
}

void main () {
	ivec3 invocID = ivec3(gl_GlobalInvocationID);
	if (any(greaterThan(invocID, dispatch_size))) return;
	_dbg_col = dbg_col;
	
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
	//float start_dist = max(interval.x - spacing*0.25, 0.0); // Move this to cpu!
	vec4 col = trace_ray(probe_pos, dir, interval.x, interval.y);
	
	vec2 ray_end = probe_pos + dir * interval.y;
	
	if (has_higher_cascade && col.a < 0.999) {
		vec4 far_col = avgerage_higher_cascade_rays(probe_pos, ray_idx, ray_end, dir);
		col = blend_light(far_col, col);
	}
	
	imageStore(out_tex, invocID, col);
}
