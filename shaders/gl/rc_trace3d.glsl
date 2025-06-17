#version 460 core
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(rgba16f, binding = 4) restrict uniform image3D out_tex;

#define DEBUGDRAW 1

#include "common.glsl"

#if DEBUGDRAW
#include "dbg_indirect_draw.glsl"

uniform vec4 dbg_col = vec4(1,0,0,1);
uniform ivec3 dbg_idx;
//uniform int dbg_ray;
#endif

#include "gpu_voxels.glsl"


uniform ivec3 dispatch_size;

uniform ivec3 world_base_pos;
uniform ivec2 world_size;
uniform float spacing;
uniform ivec2 rays_oct;

// rgb: linear light emitted along ray
// a: opacity blocking light along ray
vec4 blend_light (vec4 background, vec4 foreground) {
	return mix(background, vec4(foreground.rgb, 1.0), foreground.aaaa);
}

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
	col.rgb *= get_emmisive(bid) + 0.1;
	//col.rgb *= get_emmisive(bid);
	return col;
}

vec4 trace_ray (vec3 point, vec3 dir, float start_dist, float max_dist) {
#if DEBUGDRAW
	if (update_debugdraw) {
		ivec3 probe_idx = ivec3(ivec3(gl_GlobalInvocationID).xy / rays_oct, ivec3(gl_GlobalInvocationID).z);
		ivec2 ray_idx   =       ivec3(gl_GlobalInvocationID).xy % rays_oct;
		
		if (all(equal(probe_idx, dbg_idx))) {
			vec3 a = point + start_dist * dir;
			vec3 ab = (max_dist - start_dist) * dir;
			//vec3 a = point + 10 * dir;
			//vec3 ab = (11 - 10) * dir;
			
			dbgdraw_vector(world_base_pos + a, ab, dbg_col);
		}
	}
#endif
	
	vec3 s = point + dir * start_dist;
	return trace_ray(world_base_pos + s, dir, max_dist - start_dist);
}
vec4 trace_ray_between (vec3 start, vec3 end) {
	vec3 dir = end - start;
	float max_dist = length(dir);
	dir = normalize(dir);
	
	return trace_ray(start, dir, 0.0, max_dist);
}

void main () {
	ivec3 invocID = ivec3(gl_GlobalInvocationID);
	if (any(greaterThan(invocID, dispatch_size))) return;
	
	ivec3 probe_idx = ivec3(invocID.xy / rays_oct, invocID.z);
	ivec2 ray_idx   =       invocID.xy % rays_oct;
	
	vec3 probe_pos = spacing * (vec3(probe_idx) + 0.5);
	
	vec2 uv = vec2(ray_idx+0.5) / vec2(rays_oct);
	vec3 dir = octah2dir(uv);
	
	vec4 col = trace_ray(probe_pos, dir, 0.0, 1000);
	
	imageStore(out_tex, invocID, col);
}
