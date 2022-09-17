
#define DEBUGDRAW 1
#include "common.glsl"

#if DEBUGDRAW
	#include "dbg_indirect_draw.glsl"
#endif

#include "gpu_voxels.glsl"
#include "rand.glsl"

// Generate arbitrary tangent space
// only use for random sampling, since there's a discontinuity
vec3 generate_tangent (vec3 normal) {
	vec3 tangent = vec3(0,0,1);
	if (abs(normal.z) > 0.8) return vec3(1,0,0);
	return tangent;
}
mat3 calc_TBN (vec3 normal, vec3 tangent) {
	vec3 bitangent = cross(normal, tangent); // generate bitangent vector orthogonal to both normal and tangent
	tangent = cross(bitangent, normal); // regenerate tangent vector in case it was not orthogonal to normal
	
	return mat3(normalize(tangent), normalize(bitangent), normal);
}
mat3 generate_TBN (vec3 normal) {
	return calc_TBN(normal, generate_tangent(normal));
}

//// 
float fresnel (float dotVN, float F0) {
	float x = clamp(1.0 - dotVN, 0.0, 1.0);
	float x2 = x*x;
	return F0 + ((1.0 - F0) * x2 * x2 * x);
}
float fresnel_roughness (float dotVN, float F0, float roughness) {
	float x = clamp(1.0 - dotVN, 0.0, 1.0);
	float x2 = x*x;
	return F0 + ((max((1.0 - roughness), F0) - F0) * x2 * x2 * x);
}

vec3 hemisphere_sample () {
	// cosine weighted sampling (100% diffuse)
	// http://www.rorydriscoll.com/2009/01/07/better-sampling/
	
	// takes a uniform sample on a disc (x,y)
	// and projects into up into a hemisphere to get the cosine weighted points on the hemisphere
	
	// random sampling (Monte Carlo)
	vec2 rnd = rand2(); // uniform sample in [0,1) square
	
	// map square to disc, preserving uniformity
	float r = sqrt(rnd.y);
	float theta = 2*PI * rnd.x;
	
	float x = r * cos(theta);
	float y = r * sin(theta);
	
	// map (project) disc up to hemisphere,
	// turning uniform distribution into cosine weighted distribution
	//vec3 dir = vec3(x,y, sqrt(max(0.0, 1.0 - rnd.y)));
	vec3 dir = vec3(x,y, sqrt(1.0 - rnd.y));
	return dir;
}
vec3 hemisphere_sample_stratified (int i, int n) {
	// cosine weighted sampling (100% diffuse)
	// http://www.rorydriscoll.com/2009/01/07/better-sampling/
	
	// takes a uniform sample on a disc (x,y)
	// and projects into up into a hemisphere to get the cosine weighted points on the hemisphere
	
	// stratified sampling (Quasi Monte Carlo)
	int Nx = 4;
	ivec2 strata = ivec2(i % Nx, i / Nx);
	
	float scale = 1.0 / float(Nx);
	
	vec2 uv = rand2(); // uniform sample in [0,1) square
	//uv = (vec2(strata) + 0.5) * scale;
	uv = (vec2(strata) + uv) * scale;
	
	// map square to disc, preserving uniformity
	float r = sqrt(uv.y);
	float theta = 2*PI * uv.x;
	
	float x = r * cos(theta);
	float y = r * sin(theta);
	
	// map (project) disc up to hemisphere,
	// turning uniform distribution into cosine weighted distribution
	vec3 dir = vec3(x,y, sqrt(max(0.0, 1.0 - uv.y)));
	return dir;
}

vec3 random_in_sphere () {
	vec3 rnd = rand3();
    float theta = rnd.x * 2.0 * PI;
    float phi = acos(2.0 * rnd.y - 1.0);
    float r = pow(rnd.z, 0.33333333);
    float sp = sin(phi);
    float cp = cos(phi);
    return r * vec3(sp * sin(theta), sp * cos(theta), cp);
}

vec3 reflect_roughness (vec3 refl, vec3 normal, float roughness) {
    mat3 TBN = generate_TBN(refl); // tangent space relative to reflected vector to be able to 'offset' it by roughness
	
    float a = roughness * roughness;
    vec2 rnd = rand2(); // uniform sample in [0,1) square
	
	//float z = sqrt(clamp( (1.0 - r.y) / 1.0 + (a-1.0) * r.y, 0.0,1.0));
	float z = sqrt( (1.0 - rnd.y) / (1.0 + (a-1.0) * rnd.y) );
	
	float r = sqrt(1.0 - z*z);
	float theta = 2*PI * rnd.x;
	
	float x = r * cos(theta);
	float y = r * sin(theta);
	
    vec3 dir = TBN * vec3(x,y,z);
    return dot(dir, normal) > 0.0 ? dir : refl;
}

//// Noise function for testing, replace with set of better noise functions later
// > don't use mod289 for example, use better int based hashing instead, seems to be superior on modern gpus
// > float-based noise usually shows patterns on closer inspection

float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 mod289(vec4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 perm(vec4 x){return mod289(((x * 34.0) + 1.0) * x);}

float noise(vec3 p){
    vec3 a = floor(p);
    vec3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);

    vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
    vec4 k1 = perm(b.xyxy);
    vec4 k2 = perm(k1.xyxy + b.zzww);

    vec4 c = k2 + a.zzzz;
    vec4 k3 = perm(c);
    vec4 k4 = perm(c + 1.0);

    vec4 o1 = fract(k3 * (1.0 / 41.0));
    vec4 o2 = fract(k4 * (1.0 / 41.0));

    vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

    float val = o4.y * d.y + o4.x * (1.0 - d.y);
	
	return val * 2.0 - 1.0;
}

// Instead of executing work groups in a simple row major order
// reorder them into columns of width N (by returning a different 2d index)
// in each column the work groups are still row major order
// replicates this: https://developer.nvidia.com/blog/optimizing-compute-shaders-for-l2-locality-using-thread-group-id-swizzling/
uvec2 work_group_tiling (uint N) {
	#if 0
	return gl_WorkGroupID.xy;
	#else
	uint idx = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
	
	uint column_size       = gl_NumWorkGroups.y * N;
	uint full_column_count = gl_NumWorkGroups.x / N;
	uint last_column_width = gl_NumWorkGroups.x % N;
	
	uint column_idx    = idx / column_size;
	uint idx_in_column = idx % column_size;
	
	uint column_width = N;
	if (column_idx == full_column_count)
		column_width = last_column_width;
	
	uvec2 wg_swizzled;
	wg_swizzled.y = idx_in_column / column_width;
	wg_swizzled.x = idx_in_column % column_width + column_idx * N;
	return wg_swizzled;
	#endif
}

bool _dbgdraw = false;

void get_ray (vec2 px_pos, out vec3 ray_pos, out vec3 ray_dir) {
	//vec2 px_center = px_pos + rand2();
	vec2 px_center = px_pos + vec2(0.5);
	
	// turn [0, viewport_size] into [-1, +1]
	// equivalent to  px_center / viewport_size * 2 - 1
	vec2 uv = px_center * view.inv_viewport_size2 - vec2(1.0);
	
	// multiplying uvs with near plane basis vectors
	vec3 near_offs = view.frust_x*uv.x + view.frust_y*uv.y + view.frust_z;
	// actually normalize near_offs to get a real dir vector
	ray_dir = normalize(near_offs);
	
	// ray pos on near plane
	ray_pos = view.cam_pos + near_offs;
	
	// make relative to gpu world representation (could bake into matrix)
	ray_pos += vec3(WORLD_SIZE/2 - voxtex_pos);
}

// Convert gpu 3d texture coords to cam depth values, which can be reconstructed
// knowing the original ray_dir, to save on storage space
// NOTE: that these are not 'real' depth values
//   as we do not take the near plane into account, but it does not matter
//   this is because we only need any kind of 1d float that can help reconstruct the ray hit
float pos_to_depth (vec3 pos) {
	pos -= vec3(WORLD_SIZE/2 - voxtex_pos);
	return dot(pos - view.cam_pos, view.cam_forw);
}
vec3 depth_to_pos (vec3 ray_dir, float depth) {
	// dist from cam_pos in direction of ray_dir
	float dist = depth / dot(ray_dir, view.cam_forw);
	
	vec3 pos = ray_dir * dist + view.cam_pos;
	pos += vec3(WORLD_SIZE/2 - voxtex_pos);
	return pos;
}

// divide this by any object's z-distance to get a pixel size for LOD purposes
uniform float base_px_size;

// get pixels per meter at certain distance
// where depth is the camera-space z distance relative to the camera origin (not near plane)
float get_px_size (float depth) {
	return base_px_size / depth;
}

const float WORLD_SIZEf = float(WORLD_SIZE);
const float INV_WORLD_SIZEf = 1.0 / WORLD_SIZEf;
const int CHUNK_MASK = ~63;
const float epsilon = 0.001; // This epsilon should not round to zero with numbers up to 4096 

#if TAA_ENABLE
layout(rgba16f, binding = 1) writeonly restrict uniform  image2D taa_color;
layout(rg16ui , binding = 2) writeonly restrict uniform uimage2D taa_posage;

uniform  sampler2D taa_history_color;
uniform usampler2D taa_history_posage;

uniform mat4 prev_world2clip;
uniform int taa_max_age = 256;

vec3 APPLY_TAA (vec3 val, vec3 pos, vec3 normal, ivec2 pxpos, uint faceid) {
	pos -= vec3(WORLD_SIZEf/2 - voxtex_pos);
	
	vec4 reproj_clip = prev_world2clip * vec4(pos, 1.0);
	vec2 reproj_uv = (reproj_clip.xy / reproj_clip.w) * 0.5 + 0.5;
	
	uint age = 1u;
	if (  reproj_uv.x >= 0.0 && reproj_uv.x <= 1.0 &&
	      reproj_uv.y >= 0.0 && reproj_uv.y <= 1.0  ) {
		uvec2 sampl = textureLod(taa_history_posage, reproj_uv, 0.0).xy; // face normal, face pos, sample age
		
		uint sampl_id  = sampl.x;
		uint sampl_age = sampl.y;
		
		if (faceid == sampl_id) {
			sampl_age = min(sampl_age, uint(taa_max_age));
			age = sampl_age + 1u;
			
			vec3 accumulated = textureLod(taa_history_color, reproj_uv, 0.0).rgb * float(sampl_age);
			
			val = (accumulated + val) / float(age);
		}
	}
	
	imageStore(taa_color, pxpos, vec4(val, 0.0));
	imageStore(taa_posage, pxpos, uvec4(faceid, age, 0u, 0u));
	
	return val;
}
#else
	#define APPLY_TAA(val, pos, normal, pxpos, faceid) (val)
#endif

//
uniform int max_iterations = 200;
uniform float visualize_mult = 1.0;

#if VISUALIZE_COST
	int _iterations = 0;
	uint64_t _ts_start;
	
	uniform sampler2D heat_gradient;
	
	void INIT_VISUALIZE_COST () {
		_iterations = 0;
		
		#if VISUALIZE_TIME
		_ts_start = clockARB();
		#endif
	}
		
	void GET_VISUALIZE_COST (inout vec3 col) {
		#if VISUALIZE_TIME
		uint64_t dur = clockARB() - _ts_start;
		float val = float(dur) / (100000.0 * visualize_mult);
		//float val = float(dur) / float(_iterations) / (2000.0*visualize_mult);
		
		#else
		float val = float(_iterations) / float(max_iterations);
		#endif
		
		col = texture(heat_gradient, vec2(val, 0.5)).rgb;
	}
	
	#define VISUALIZE_ITERATION ++_iterations;
	
#else
	#define INIT_VISUALIZE_COST()
	#define GET_VISUALIZE_COST(col)
	#define VISUALIZE_ITERATION
#endif

struct Hit {
	vec3	pos;
	float	dist;
	vec3	normal; // normal mapped normal
	vec3	gnormal; // non-normal mapped real geometry (uv-compatible) TBN
	
	//ivec3	coord;
	uint	faceid;
	uint	bid;
	
	vec4	col;
	vec3	emiss;
	float	emiss_raw;
};

#if BEVEL
uint read_voxel_bevel_type (ivec3 coord) {
	uint bid = texelFetch(voxel_tex, coord, 0).r;
	if (bid == B_GRASS) bid = B_EARTH;
	return bid;
}

void plane_intersect (inout float t0, inout float t1, inout vec3 bevel_normal,
		vec3 ray_pos, vec3 ray_dir, vec3 plane_norm, float plane_d) {
	plane_norm = normalize(plane_norm);
	
	float dpos  = dot(ray_pos, plane_norm) + plane_d;
	float dnorm = dot(ray_dir, plane_norm);
	
	float t = dpos / dnorm;
	
	if (dnorm < 0.0) {
		if (t > t0) bevel_normal = plane_norm;
		t0 = max(t0, t);
	}
	if (dnorm > 0.0) t1 = min(t1, t);
}
bool box_bevel (vec3 ray_pos, vec3 ray_dir, ivec3 coord,
		inout float t0, in float t1, inout vec3 norm) {
	
	vec3 origin = vec3(coord) + 0.5;
	vec3 rel = origin - ray_pos;
	
	//vec3 s = sign((ray_dir * t0 + ray_pos) - origin);
	vec3 s = mix(vec3(-1), vec3(+1), greaterThan(ray_dir * t0, rel)); // faster than above
	
	uint v0   = read_voxel_bevel_type(coord).r;
	bool v100 = read_voxel_bevel_type(coord + ivec3(s.x,  0,  0)).r != v0;
	bool v010 = read_voxel_bevel_type(coord + ivec3(  0,s.y,  0)).r != v0;
	bool v001 = read_voxel_bevel_type(coord + ivec3(  0,  0,s.z)).r != v0;
	
	bool v011 = read_voxel_bevel_type(coord + ivec3(  0,s.y,s.z)).r != v0;
	bool v101 = read_voxel_bevel_type(coord + ivec3(s.x,  0,s.z)).r != v0;
	bool v110 = read_voxel_bevel_type(coord + ivec3(s.x,s.y,  0)).r != v0;
	
	float corn = -1.0;
	if (v100 && v010 && v001) corn = 0.18; // bevel for blocks with 3 air blocks around
	else {
		// make concave edge bevel when multiple beveled edges meet
		if ((!v100 && !v010 && !v001  && v011 && v101 && v110) ||
	        ( v100 && !v010 && !v001  && v011) ||
	        (!v100 &&  v010 && !v001  && v101) ||
	        (!v100 && !v010 &&  v001  && v110)) corn = 0.05715;
	}
	
	float rot = 0.0;
	
	float szcorner = HALF_SQRT_3 - corn;
	float szedge   = HALF_SQRT_2 - 0.07;
	
	if (corn > -1.0)  plane_intersect(t0,t1, norm, rel, ray_dir, s, szcorner);
	if (v010 && v001) plane_intersect(t0,t1, norm, rel, ray_dir, s * vec3(rot,1,1), szedge);
	if (v100 && v001) plane_intersect(t0,t1, norm, rel, ray_dir, s * vec3(1,rot,1), szedge);
	if (v100 && v010) plane_intersect(t0,t1, norm, rel, ray_dir, s * vec3(1,1,rot), szedge);
	
	if (t0 > t1) {
		norm = vec3(0.0);
		return false;
	}
	return true;
}
#endif

bool trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit,
		vec3 raycol, bool primray) {
	
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
		vec3 world_min = vec3(        0.0) + epsilon;
		vec3 world_max = vec3(WORLD_SIZEf) - epsilon;
		
		// calculate entry and exit coords into whole world cube
		vec3 t0v = mix(world_max, world_min, dir_sign) * inv_dir + bias;
		vec3 t1v = mix(world_min, world_max, dir_sign) * inv_dir + bias;
		float t0 = max( max(max(t0v.x, t0v.y), t0v.z), 0.0);
		float t1 = max( min(min(t1v.x, t1v.y), t1v.z), 0.0);
		
		// ray misses world texture
		if (t1 <= t0)
			return false;
		
		// adjust ray to start where it hits cube initally
		dist = t0;
		max_dist = min(t1, max_dist);
	}
	
	// step epsilon less than 1m to possibly avoid somtimes missing one voxel cell when DF stepping
	float manhattan_fac = (1.0 - epsilon) / (abs(ray_dir.x) + abs(ray_dir.y) + abs(ray_dir.z));
	
	vec3 pos = dist * ray_dir + ray_pos;
	ivec3 coord = ivec3(pos);
	
	int dbgcol = 0;
	int iter = 0;
	
	vec3 bevel_normal = vec3(0.0);
	
	for (;;) {
		VISUALIZE_ITERATION
		
		int dfi = texelFetch(df_tex, coord, 0).r;
		
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
			
		//#if DEBUGDRAW
		//	pos = dist * ray_dir + ray_pos; // fix pos not being updated after DDA (just for dbg)
		//	vec4 col = dbgcol==0 ? vec4(1,0,0,1) : vec4(0.8,0.2,0,1);
		//	if (_dbgdraw) dbgdraw_wire_sphere(pos - WORLD_SIZEf/2.0, vec3(df*2.0), col);
		//	if (_dbgdraw) dbgdraw_point(      pos - WORLD_SIZEf/2.0,      df*0.5 , col);
		//#endif
			
			// compute chunk exit, since DF is not valid for things outside of the chunk it is generated for
			vec3 chunk_exit = vec3(coord & CHUNK_MASK) + chunk_exit_planes;
			
			vec3 chunk_t1v = inv_dir * chunk_exit + bias;
			float chunk_t1 = min(min(chunk_t1v.x, chunk_t1v.y), chunk_t1v.z);
			
			dist += df;
			dist = min(dist, chunk_t1); // limit step to exactly on the exit face of the chunk
			
			vec3 pos = dist * ray_dir + ray_pos;
			// update coord for next iteration
			coord = ivec3(pos);
		} else {
			// we need to check individual voxels by DDA now
			
		//#if DEBUGDRAW
		//	if (_dbgdraw) dbgdraw_wire_cube(vec3(coord) + 0.5 - WORLD_SIZEf/2.0, vec3(1.0), vec4(1,1,0,1));
		//#endif
			
			vec3 t1v = inv_dir * vec3(coord + vox_exit) + bias;
			float t1 = min(min(t1v.x, t1v.y), t1v.z);
			
			if (dfi < 0) {
			#if BEVEL
				if (box_bevel(ray_pos, ray_dir, coord, dist, t1, bevel_normal))
			#endif // !primray || 
					break; // hit
			}
			
			dist = t1;
			
			// step on axis where exit distance is lowest
			if      (t1v.x == t1) coord.x += step_dir.x;
			else if (t1v.y == t1) coord.y += step_dir.y;
			else                  coord.z += step_dir.z;
		}
		
		dbgcol ^= 1;
		iter++;
		if (iter >= max_iterations || dist >= max_dist)
			break; // miss
	}
	
	#if DEBUGDRAW
	if (_dbgdraw)
		dbgdraw_vector(ray_pos - WORLD_SIZEf/2.0, ray_dir * dist, vec4(raycol,1));
	#endif
	
	if (iter >= max_iterations || dist >= max_dist)
		return false; // miss
	
	{ // calc hit info
		
		hit.bid = texelFetch(voxel_tex, coord, 0).r;
		hit.dist = dist;
		hit.pos = dist * ray_dir + ray_pos;
		//hit.coord = coord;
		
		vec2 uv;
		int face;
		{ // calc hit face, uv and normal
			vec3 rel = hit.pos - vec3(coord);
			vec3 offs = rel - 0.5;
			vec3 abs_offs = abs(offs);
			
			hit.gnormal = vec3(0.0);
			vec3 tang = vec3(0.0);
			
			
			if (abs_offs.x >= abs_offs.y && abs_offs.x >= abs_offs.z) {
				face = rel.x < 0.5 ? 0 : 1;
				
				hit.faceid  = face << 13;
				hit.faceid |= ~(2<<13) & uint(coord.x);
				
				hit.gnormal.x = sign(offs.x);
				tang.y = offs.x < 0.0 ? -1 : +1;
				
				uv = offs.x < 0.0 ? vec2(1.0-rel.y, rel.z) : vec2(rel.y, rel.z);
			} else if (abs_offs.y >= abs_offs.z) {
				face = rel.y < 0.5 ? 2 : 3;
				
				hit.faceid  = face << 13;
				hit.faceid |= ~(2<<13) & uint(coord.y);
				
				hit.gnormal.y = sign(offs.y);
				tang.x = offs.y < 0.0 ? +1 : -1;
				
				uv = offs.y < 0.0 ? vec2(rel.x, rel.z) : vec2(1.0-rel.x, rel.z);
			} else {
				face = rel.z < 0.5 ? 4 : 5;
				
				hit.faceid  = face << 13;
				hit.faceid |= ~(2<<13) & uint(coord.z);
				
				hit.gnormal.z = sign(offs.z);
				tang.x = +1;
				
				uv = offs.z < 0.0 ? vec2(rel.x, 1.0-rel.y) : vec2(rel.x, rel.y);
			}
			
			bool was_bevel = dot(bevel_normal, bevel_normal) != 0.0;
			if (was_bevel) hit.gnormal = bevel_normal;
			
			hit.normal = hit.gnormal;
			mat3 gTBN = calc_TBN(hit.gnormal, tang);
		}
		
		uint medium_bid = B_AIR;
		
		uint tex_bid = hit.bid;
		if  (hit.bid == B_AIR) tex_bid = medium_bid; 
		
		float texid = float(block_tiles[tex_bid].sides[face]);
		
		hit.col = textureLod(tile_textures, vec3(uv, texid), 0.0).rgba;
		
		//hit.col = vec4(vec3(dist / 100.0), 1);
		
		hit.emiss_raw = get_emmisive(tex_bid);
		hit.emiss = hit.col.rgb * hit.emiss_raw;
	}
	
	return true; // hit
}


uniform float vct_stepsize = 1.0;
uniform float test;

vec4 read_vct_texture (vec3 texcoord, vec3 dir, float size) {
	
#if 1
	
	// Since we are heavily bottlenecked by memory access
	// it is actually faster to branch to minimize cache trashing
	// by special-casing LOD0 (since all 6 directions are identical)
	
	if (size <= 1.0) {
		#if 1
		// prevent small samples from being way too blurry
		// -> simulate negative lods (size < 1.0) by snapping texture coords to nearest texels
		// when approaching size=0
		// size = 0.5 would snap [0.25,0.75] to 0.5
		//              and lerp [0.75,1.25] in [0.5,1.5]
		texcoord = 0.5 * size + texcoord;
		texcoord = min(fract(texcoord) * (1.0 / size) - 0.5, 0.5) + floor(texcoord);
		//texcoord = floor(texcoord);
		#endif
		texcoord *= INV_WORLD_SIZEf;
		
		return textureLod(vct_tex_mip0, texcoord, 0.0) * VCT_UNPACK;
	}
	else {
		float lod = log2(size)-1; // -1 because of seperate mip0 texture
		
		texcoord *= INV_WORLD_SIZEf;
		
		vec4 valX = textureLod(dir.x < 0.0 ? vct_texNX : vct_texPX, texcoord, lod);
		vec4 valY = textureLod(dir.y < 0.0 ? vct_texNY : vct_texPY, texcoord, lod);
		vec4 valZ = textureLod(dir.z < 0.0 ? vct_texNZ : vct_texPZ, texcoord, lod);
		
		vec3 sqr = dir * dir;
		vec4 val = (valX*sqr.x + valY*sqr.y + valZ*sqr.z) * VCT_UNPACK;
		
		//vec3 weight = min(abs(dir) * 15.0, 1.0);
		//valX.a *= weight.x;
		//valY.a *= weight.y;
		//valZ.a *= weight.z;
		
		//val.a = max(max(valX.a, valY.a), valZ.a);
		
		return val;
	}
#else
	float lod = log2(size);
	
	// prevent small samples from being way too blurry
	// -> simulate negative lods (size < 1.0) by snapping texture coords to nearest texels
	// when approaching size=0
	// size = 0.5 would snap [0.25,0.75] to 0.5
	//              and lerp [0.75,1.25] in [0.5,1.5]
	if (size <= 1.0) {
		texcoord = 0.5 * size + texcoord;
		texcoord = min(fract(texcoord) * (1.0 / size) - 0.5, 0.5) + floor(texcoord);
	}
	
	texcoord *= INV_WORLD_SIZEf;
	
	return textureLod(vct_texNX, texcoord, lod) * VCT_UNPACK;
#endif
}
vec4 trace_cone (vec3 cone_pos, vec3 cone_dir, float cone_slope, float start_dist, float max_dist, bool dbg) {
	
	float dist = start_dist;
	
	vec3 color = vec3(0.0);
	float transp = 1.0; // inverse alpha to support alpha stepsize fix
	
	for (int i=0; i<4000; ++i) {
		if (gl_LocalInvocationID.z == 0) {
			VISUALIZE_ITERATION
		}
		
		vec3 pos = cone_pos + cone_dir * dist;
		float size = cone_slope * 2.0 * dist;
		
		float stepsize = size * vct_stepsize;
		
		vec4 sampl = read_vct_texture(pos, cone_dir, size);
		
		vec3 emiss = sampl.rgb;
		float through = 1.0 - sampl.a;
		
		// correct sample for step sizes <1, since they are no longer preintegrated like the larger LODs
		if (stepsize < 1.0) {
			// correct emmission samples
			// because we accumulate it 1/stepsize too often
			// emiss *= stepsize
			// TODO: this totally breaks with alpha close to 1 since only one or a few samples are taken
			//  yet the emiss is usually supposed to be the same irrelevant of stepsize
			
			//emiss *= stepsize;
			
			float a = 0.1;
			float b = 0.8;
			emiss *= mix(1.0, stepsize, clamp((through - a) / (b-a), 0.0, 1.0));
			
			// correct alpha sample
			// transp accumulation can be thought of (1-alpha)^x
			// so with stepsize=0.5 it's equivalent to multiplying twice as much -> (1-alpha)^2x
			// so to correct we do ((1-alpha)^1/2)^2x
			through = pow(through, stepsize);
		}
		
		vec3 new_col = color + transp * emiss;
		float new_transp = transp * through;
		
		#if DEBUGDRAW
		if (_dbgdraw && dbg) {
			//vec4 col = vec4(1,0,0,1);
			//vec4 col = vec4(sampl.rgb, 1.0-transp);
			//vec4 col = vec4(vec3(sampl.a), transp);
			vec4 col = vec4(vec3(sampl.a), 1.0);
			dbgdraw_wire_cube(pos - WORLD_SIZEf/2.0, vec3(size), col);
		}
		#endif
		
		color = new_col;
		transp = new_transp;
		
		dist += stepsize;
		
		if (transp < 0.002 || dist >= max_dist)
			break;
	}
	
	//return vec4(vec3(dist / 300.0), 1.0);
	//return vec4(vec3(transp), 1.0);
	return vec4(color, 1.0 - transp);
}

