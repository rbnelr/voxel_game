
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
	
	uint column_idx = idx / column_size;
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

// get pixel ray in world space based on pixel coord and matricies
uniform float near_px_size;

void get_ray (vec2 px_pos, out vec3 ray_pos, out vec3 ray_dir) {
	//vec2 px_center = px_pos + rand2();
	vec2 px_center = px_pos + vec2(0.5);
	
	vec2 uv = px_center / view.viewport_size * 2.0 - 1.0;
	
	ray_dir = view.frust_x*uv.x + view.frust_y*uv.y + view.frust_z;
	ray_pos = view.cam_pos + ray_dir;
	
	ray_dir = normalize(ray_dir);
	
	// make relative to gpu world representation (could bake into matrix)
	ray_pos += float(WORLD_SIZE/2);
}

const float WORLD_SIZEf = float(WORLD_SIZE);
const float INV_WORLD_SIZEf = 1.0 / WORLD_SIZEf;
const int CHUNK_MASK = ~63;
const float epsilon = 0.001; // This epsilon should not round to zero with numbers up to 4096 

#if TAA_ENABLE
layout(rgba16f , binding = 1) writeonly restrict uniform  image2D taa_color;
layout(rgba16ui, binding = 2) writeonly restrict uniform uimage2D taa_posage;

uniform  sampler2D taa_history_color;
uniform usampler2D taa_history_posage;

uniform mat4 prev_world2clip;
uniform int taa_max_age = 256;

vec3 APPLY_TAA (vec3 val, vec3 pos, ivec3 coord, vec3 normal, ivec2 pxpos) {
	uint age = 1u;
	
	vec4 prev_clip = prev_world2clip * vec4(pos - WORLD_SIZEf/2, 1.0);
	prev_clip.xyz /= prev_clip.w;
	
	uint cur_norm = 5;
	if      (normal.x < -0.5) cur_norm = 0;
	else if (normal.x > +0.5) cur_norm = 1;
	else if (normal.y < -0.5) cur_norm = 2;
	else if (normal.y > +0.5) cur_norm = 3;
	else if (normal.z < -0.5) cur_norm = 4;
	else if (normal.z > +0.5) cur_norm = 5;
	
	uint cur_pos = 0;
	if      (cur_norm/2 == 0) cur_pos = (uint)coord.x;
	else if (cur_norm/2 == 1) cur_pos = (uint)coord.y;
	else if (cur_norm/2 == 2) cur_pos = (uint)coord.z;
	
	vec2 uv = prev_clip.xy * 0.5 + 0.5;
	if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0) {
		uvec4 sampl = textureLod(taa_history_posage, uv, 0.0);
		
		//vec3 sampl_pos = vec3(sampl.rgb) / float(0xffff) * WORLD_SIZEf;
		uint sampl_norm = sampl.r; // 0=-X 1=+X 2=-Y ...
		uint sampl_pos = sampl.g; // relative to normal
		uint sampl_age = sampl.a;
		
		//if (distance(pos, sampl_pos) < 0.1) {
		if (cur_norm == sampl_norm && cur_pos == sampl_pos) {
			age = min(sampl_age, uint(taa_max_age));
			
			vec3 accumulated = textureLod(taa_history_color, uv, 0.0).rgb * float(age);
			
			age += 1u;
			val = (accumulated + val) / float(age);
		}
	}
	
	//uvec3 pos_enc = uvec3(round(pos * float(0xffff) / WORLD_SIZEf));
	//pos_enc = clamp(pos_enc, uvec3(0), uvec3(0xffff));
	
	imageStore(taa_color, pxpos, vec4(val, 0.0));
	imageStore(taa_posage, pxpos, uvec4(cur_norm, cur_pos, 0u, age));
	
	return val;
}
#else
	#define APPLY_TAA(val, pos, coord, normal, pxpos) (val)
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
	
	ivec3	coord;
	uint	bid;
	
	vec4	col;
	vec3	emiss;
	float	emiss_raw;
};

uniform sampler2DArray test_cubeN;
uniform sampler2DArray test_cubeH;

const float tex = 2.0;

#if BEVEL
uint read_voxel_bevel_type (ivec3 coord) {
	uint bid = texelFetch(voxel_tex, coord, 0).r;
	if (bid == B_GRASS) bid = B_EARTH;
	return bid;
}

void plane_intersect (inout float t0, inout float t1, inout vec3 bevel_normal,
		vec3 ray_pos, vec3 ray_dir, vec3 plane_norm, float plane_d) {
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
		inout float t0, in float t1, inout vec3 bevel_normal) {
	
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
	if (v100 && v010 && v001) corn = 0.17; // bevel for blocks with 3 air blocks around
	else {
		// make concave edge bevel when multiple beveled edges meet
		if ((!v100 && !v010 && !v001  && v011 && v101 && v110) ||
	        ( v100 && !v010 && !v001  && v011) ||
	        (!v100 &&  v010 && !v001  && v101) ||
	        (!v100 && !v010 &&  v001  && v110)) corn = 0.05715;
	}
	
	//float rot = 0.04 * lod;
	float rot = 0.0;
	
	vec3 edgeXn = normalize(s * vec3(rot,1,1));
	vec3 edgeYn = normalize(s * vec3(1,rot,1));
	vec3 edgeZn = normalize(s * vec3(1,1,rot));
	vec3 cornern = normalize(s);
	
	float szcorner = HALF_SQRT_3 - corn;
	float szedge   = HALF_SQRT_2 - 0.07;
	
	if (corn > -1.0)  plane_intersect(t0,t1, bevel_normal, rel, ray_dir, cornern, szcorner);
	if (v010 && v001) plane_intersect(t0,t1, bevel_normal, rel, ray_dir, edgeXn, szedge);
	if (v100 && v001) plane_intersect(t0,t1, bevel_normal, rel, ray_dir, edgeYn, szedge);
	if (v100 && v010) plane_intersect(t0,t1, bevel_normal, rel, ray_dir, edgeZn, szedge);
	
	if (t0 > t1) {
		bevel_normal = vec3(0.0);
		return false;
	}
	return true;
}
#endif

uniform int test_lod = 2;

bool trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit,
		vec3 raycol, bool primray) {
	max_dist = 400.0;
	
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
	
#if 0
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
				if (primray || box_bevel(ray_pos, ray_dir, coord, dist, t1, bevel_normal))
			#endif
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
#else
	int lodsz = 1<<test_lod;
	
	coord &= ~(lodsz-1);
	
	vec3 t1v = inv_dir * vec3(coord + vox_exit*lodsz) + bias;
	vec3 tstep = inv_dir * mix(vec3(-1), vec3(+1), dir_sign);
	
	step_dir *= lodsz;
	tstep *= lodsz;
	
	vec4 accum = vec4(0.0);
	int axis = 0;
	ivec3 faces = mix(ivec3(0,2,4), ivec3(1,3,5), dir_sign);
	
	for (;;) {
		VISUALIZE_ITERATION
		
		#if DEBUGDRAW
		if (_dbgdraw) dbgdraw_wire_cube(vec3(coord) + float(lodsz)/2.0 - WORLD_SIZEf/2.0, vec3(lodsz), vec4(1,1,0,1));
		#endif
		
		//int dfi = texelFetch(df_tex, coord, 0).r;
		vec4 vct = texelFetch(vct_tex[faces[axis]], coord>>test_lod, test_lod) * VCT_UNPACK;
		
		accum.rgb += (1.0 - accum.a) * vct.rgb;
		accum.a   += (1.0 - accum.a) * vct.a;
		
		if (accum.a > 0.98)
			break;
		
		dist = min(min(t1v.x, t1v.y), t1v.z);
		
		if (++iter >= max_iterations || dist >= max_dist)
			break;
		
		// step on axis where exit distance is lowest
		if      (t1v.x == dist) { coord.x += step_dir.x; t1v.x += tstep.x; axis = 0; }
		else if (t1v.y == dist) { coord.y += step_dir.y; t1v.y += tstep.y; axis = 1; }
		else                    { coord.z += step_dir.z; t1v.z += tstep.z; axis = 2; }
	}
	
	#if DEBUGDRAW
	if (_dbgdraw)
		dbgdraw_vector(ray_pos - WORLD_SIZEf/2.0, ray_dir * dist, vec4(raycol,1));
	#endif
	
	hit.col = vec4(accum.rgb, 1.0) * accum.a;
	
	return true;
#endif
	
	{ // calc hit info
		
		hit.bid = texelFetch(voxel_tex, coord, 0).r;
		hit.dist = dist;
		hit.pos = dist * ray_dir + ray_pos;
		hit.coord = coord;
		
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
				
				hit.gnormal.x = sign(offs.x);
				tang.y = offs.x < 0.0 ? -1 : +1;
				
				uv = offs.x < 0.0 ? vec2(1.0-rel.y, rel.z) : vec2(rel.y, rel.z);
			} else if (abs_offs.y >= abs_offs.z) {
				face = rel.y < 0.5 ? 2 : 3;
				
				hit.gnormal.y = sign(offs.y);
				tang.x = offs.y < 0.0 ? +1 : -1;
				
				uv = offs.y < 0.0 ? vec2(rel.x, rel.z) : vec2(1.0-rel.x, rel.z);
			} else {
				face = rel.z < 0.5 ? 4 : 5;
				
				hit.gnormal.z = sign(offs.z);
				tang.x = +1;
				
				uv = offs.z < 0.0 ? vec2(rel.x, 1.0-rel.y) : vec2(rel.x, rel.y);
			}
			
			bool was_bevel = dot(bevel_normal, bevel_normal) != 0.0;
			if (was_bevel) hit.gnormal = bevel_normal;
			
			hit.normal = hit.gnormal;
			mat3 gTBN = calc_TBN(hit.gnormal, tang);
			
		#if NORMAL_MAPPING
			if (!was_bevel) { // normal mapping
				const float normal_map_stren = 1.0;
				
				vec3 sampl = textureLod(test_cubeN, vec3(uv, tex), 0.0).rgb;
				sampl = sampl * 2.0 - 1.0;
				sampl.xy *= normal_map_stren;
				
				//hit.normal = hit.gTBN * vec3(sampl, sqrt(1.0 - sampl.x*sampl.x - sampl.y*sampl.y));
				hit.normal = hit.gTBN * normalize(sampl);
			}
		#endif
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
