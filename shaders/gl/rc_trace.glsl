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


// rgb: linear light emitted along ray
// a: opacity blocking light along ray
vec4 blend_light (vec4 background, vec4 foreground) {
	return mix(background, vec4(foreground.rgb, 1.0), foreground.aaaa);
}

#define BILINEAR_FIX 1
#define ACCURATE_TRACING 1

#if !ACCURATE_TRACING
//// Shitty raymarched implementation due since I could not be bothered intially

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
			
			dbgdraw_vector(world_base_pos + vec3(a.x, .95, a.y), vec3(ab.x, 0, ab.y), dbg_col);
			
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
		
		cur_dist += min(spacing, 0.05); // raymarch at half voxel res, seems to help with aliasing
	}
	return col;
}
#else
//// Accurate voxel raytracing taken from my path tracer

const float INV_WORLD_SIZEf = 1.0 / WORLD_SIZEf;
const int CHUNK_MASK = ~63;
const float epsilon = 0.001; // This epsilon should not round to zero with numbers up to 4096 
uniform int max_iterations = 200;

vec4 trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist) {
	
	bvec3 dir_sign = greaterThanEqual(ray_dir, vec3(0.0));
	
	ivec3 step_dir = mix(ivec3(-1), ivec3(+1), dir_sign);
	ivec3 vox_exit = mix(ivec3(0), ivec3(1), dir_sign);
	
	// project to chunk bounds that are epsilon futher out to avoid ray getting stuck when exiting chunk
	// NOTE: this means we can sometimes step along the diagonal between chunks, missing cells
	vec3 chunk_exit_planes = mix(vec3(-epsilon), vec3(64.0 + epsilon), dir_sign);
	
	// precompute part of plane projection equation
	// prefer  'pos * inv_dir + bias'  over  'inv_dir * (pos - ray_pos)'
	// due to madd instruction
	vec3 inv_dir = 1.0 / ray_dir;
	vec3 bias = inv_dir * -ray_pos;
	
	float dist;
	{ // allow ray to start outside of world texture cube for nice debugging views
		vec3 world_min = voxtex_world_min                     + epsilon;
		vec3 world_max = voxtex_world_min + vec3(WORLD_SIZEf) - epsilon;
		
		// calculate entry and exit coords into whole world cube
		vec3 t0v = mix(world_max, world_min, dir_sign) * inv_dir + bias;
		vec3 t1v = mix(world_min, world_max, dir_sign) * inv_dir + bias;
		float t0 = max( max(max(t0v.x, t0v.y), t0v.z), 0.0);
		float t1 = max( min(min(t1v.x, t1v.y), t1v.z), 0.0);
		
		// ray misses world texture
		if (t1 <= t0)
			return vec4(0); // miss
		
		// adjust ray to start where it hits cube initally
		dist = t0;
		max_dist = min(t1, max_dist);
	}
	
	// step epsilon less than 1m to possibly avoid somtimes missing one voxel cell when DF stepping
	float manhattan_fac = (1.0 - epsilon) / (abs(ray_dir.x) + abs(ray_dir.y) + abs(ray_dir.z));
	
	vec3 pos = dist * ray_dir + ray_pos;
	ivec3 coord = ivec3(floor(pos));
	
	int iter = 0;
	
	for (;;) {
		int dfi = texelFetch(df_tex, (coord) & WORLD_SIZE_MASK, 0).r;
		
		// step up to exit of current cell, since DF is safe up until its bounds
		// seems to give a little bit of perf, as this reduces iteration count
		// of course iteration now has more instructions, so could hurt as well
		// -> disable for now, we save iterations, but it gets slower in almost every case
		//vec3 t1v = inv_dir * vec3(coord + vox_exit) + bias;
		//dist = min(min(t1v.x, t1v.y), t1v.z);
		
		if (dfi > 1) {
			float df = float(dfi) * manhattan_fac;
			
			// DF tells us that we can still step by <df> before we could possibly hit a voxel
			// step via DF raymarching
			
			// compute chunk exit, since DF is not valid for things outside of the chunk it is generated for
			// TODO: could cache chunk_t1 and simply recompute if dist >= chunk_t1
			vec3 chunk_exit = vec3(coord & CHUNK_MASK) + chunk_exit_planes;
			
			vec3 chunk_t1v = inv_dir * chunk_exit + bias;
			float chunk_t1 = min(min(chunk_t1v.x, chunk_t1v.y), chunk_t1v.z);
			
			dist += df;
			dist = min(dist, chunk_t1); // limit step to exactly on the exit face of the chunk
			
			vec3 pos = dist * ray_dir + ray_pos;
			// update coord for next iteration
			//coord = ivec3(pos);
			coord = ivec3(floor(pos));
		} else {
			// we need to check individual voxels by DDA now
			
			vec3 t1v = inv_dir * vec3(coord + vox_exit) + bias;
			float t1 = min(min(t1v.x, t1v.y), t1v.z);
			
			if (dfi < 0) {
				break; // hit
			}
			
			dist = t1;
			
			// step on axis where exit distance is lowest
			if      (t1v.x == t1) coord.x += step_dir.x;
			else if (t1v.y == t1) coord.y += step_dir.y;
			else                  coord.z += step_dir.z;
		}
		
		iter++;
		if (iter >= max_iterations || dist >= max_dist)
			break; // miss
	}
	
	if (iter >= max_iterations || dist >= max_dist)
		return vec4(0); // miss
	
	uint bid = texelFetch(voxel_tex, coord & WORLD_SIZE_MASK, 0).r;
	
	float texid = float(block_tiles[bid].sides[1]);
	vec4 col = bid <= B_AIR ? vec4(0) : textureLod(tile_textures, vec3(0.5,0.5, texid), 99.0).rgba;
	col.rgb *= get_emmisive(bid);
	return col;
}

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
			
			//dbgdraw_vector(world_base_pos + vec3(a.x, .95, a.y), vec3(ab.x, 0, ab.y), dbg_col);
		}
	}
#endif
	
	vec2 s = point + dir * start_dist;
	return trace_ray(world_base_pos + vec3(s.x, 0.5, s.y), vec3(dir.x, 0.0, dir.y), max_dist - start_dist);
}
#endif

vec4 trace_ray_between (vec2 start, vec2 end) {
	vec2 dir = end - start;
	float max_dist = length(dir);
	dir = normalize(dir);
	
	return trace_ray(start, dir, 0.0, max_dist);
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
	
	#if !BILINEAR_FIX
	vec4 col = trace_ray(probe_pos, dir, interval.x, interval.y);
	
	if (has_higher_cascade) {
		// Hardware bilinear filtering without bilinear fix
		float hi_spacing = spacing * scale_factor;
		vec2 hi_probe_coord = probe_pos / hi_spacing - 0.5;
		float hi_rays = float(ray_idx) * branching_factor;
		
		vec3 texsz = 1.0 / textureSize(higher_cascade, 0);
		
		// While it is kind of obvious in this version, I don't pre-average
		// since my understanding is that if you store all rays, you can later evaluate hemispheres for 3d surface lighting or even attempt to extract specular light
		vec4 far_col = vec4(0);
		for (float ray=hi_rays; ray < hi_rays + branching_factor; ray++) {
			vec3 uv = (vec3(hi_probe_coord, ray) + 0.5) * texsz;
			far_col += texture(higher_cascade, uv);
		}
		far_col /= branching_factor;
		
		col = blend_light(far_col, col);
	}
	#else
	vec4 col;
	if (has_higher_cascade) {
		vec2 ray_start = probe_pos + dir * interval.x;
		
		// parent cascade probe spacing and ray count
		float hi_spacing = spacing * scale_factor;
		int hi_rays = ray_idx * int(branching_factor);
		// parent probe indices and bilinear factors based on this probe's position
		vec2 hi_probe_coordF = probe_pos / hi_spacing - 0.5;
		vec2 tmpF = floor(hi_probe_coordF);
		ivec2 hi_probe_coord = ivec2(tmpF);
		vec2 bilin = hi_probe_coordF - tmpF;
		
		vec2 hi_probe_pos = hi_spacing * (tmpF + 0.5);
		vec2 hi_rays_start = hi_probe_pos + hi_interval.x * dir; // reuse this probe dir
		
	//#if DEBUGDRAW
	//	ivec2 coord = ivec2(round(dbg_pos / spacing - 0.5));
	//	ivec2 diff = (coord - probe_coord); // abs
	//	if ( cascade <= 4 && ray_idx == 0 &&
	//		  diff.x >= -1 && diff.x <= 1 &&
	//		  diff.y >= -1 && diff.y <= 1  ) {
	//		dbgdraw_point(world_base_pos + vec3(probe_pos.x, .95, probe_pos.y), 0.03, dbg_col);
	//	}
	//#endif
		
		// Bilinear fix implementation
		// Cast ray from probe interval start to approx center of 4 parent probe interval starts
		vec4 close00 = trace_ray_between(ray_start, hi_rays_start);
		vec4 close10 = trace_ray_between(ray_start, hi_rays_start + vec2(hi_spacing, 0.0));
		vec4 close01 = trace_ray_between(ray_start, hi_rays_start + vec2(0.0, hi_spacing));
		vec4 close11 = trace_ray_between(ray_start, hi_rays_start + vec2(hi_spacing, hi_spacing));
		
		// Manual bilinear sample (with no pre-averaging)
		vec4 far00 = vec4(0);
		vec4 far10 = vec4(0);
		vec4 far01 = vec4(0);
		vec4 far11 = vec4(0);
		for (int ray=hi_rays; ray < hi_rays + int(branching_factor); ray++) {
			ivec3 P = ivec3(hi_probe_coord, ray);
			far00 += texelFetchOffset(higher_cascade, P, 0, ivec3(0,0,0));
			far10 += texelFetchOffset(higher_cascade, P, 0, ivec3(1,0,0));
			far01 += texelFetchOffset(higher_cascade, P, 0, ivec3(0,1,0));
			far11 += texelFetchOffset(higher_cascade, P, 0, ivec3(1,1,0));
		}
		far00 *= 1.0 / branching_factor;
		far10 *= 1.0 / branching_factor;
		far01 *= 1.0 / branching_factor;
		far11 *= 1.0 / branching_factor;
		
		vec4 c00 = blend_light(far00, close00);
		vec4 c10 = blend_light(far10, close10);
		vec4 c01 = blend_light(far01, close01);
		vec4 c11 = blend_light(far11, close11);
		
		col = mix(mix(c00, c10, bilin.x),
		          mix(c01, c11, bilin.x), bilin.y);
		//col = (c00 + c10 + c01 + c11) * 0.25;
		//col = vec4(bilin, 0, 1);
	}
	else {
		col = trace_ray(probe_pos, dir, interval.x, interval.y);
	}
	#endif
	
	imageStore(out_tex, invocID, col);
}
