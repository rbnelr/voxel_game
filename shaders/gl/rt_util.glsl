
#include "common.glsl"
#include "gpu_voxels.glsl"
#include "rand.glsl"

#define PI	3.1415926535897932384626433832795
#define INF (1. / 0.)

#if VISUALIZE_COST
int iterations = 0;

uniform int max_iterations = 200;

uniform sampler2D		heat_gradient;
	#if VISUALIZE_WARP_COST
		#define WARP_COUNT_IN_WG ((LOCAL_SIZE_X*LOCAL_SIZE_Y) / 32) 
		shared uint warp_iter[WARP_COUNT_IN_WG];
	#endif
#endif

#define DEBUG_RAYS 0
#if DEBUG_RAYS
bool _dbg_ray = false;
uniform bool update_debug_rays = false;
#endif

#define REFLECTIONS 1

//
struct Hit {
	vec3	pos;
	vec3	normal;
	float	dist;
	uint	bid;
	uint	prev_bid;
	vec3	col;
	vec3	emiss;
};

const float WORLD_SIZEf = float(WORLD_SIZE);
const float INV_WORLD_SIZEf = 1.0 / float(WORLD_SIZE);

const uint SIGNEXP_MASK     = 0xff800000u;
const uint MANTISSA_MASK    = 0x007fffffu;

const int MANTISSA_BITS = 23;
const int MANTISSA_SHIFT = MANTISSA_BITS - OCTREE_MIPS;

const uint ROUND_MASK = 0xffffffffu << MANTISSA_SHIFT;
const uint VOXEL_SIZE = 1u << MANTISSA_SHIFT;

#define TOUINT floatBitsToUint
#define TOFLOAT uintBitsToFloat

bool trace_ray (vec3 pos, vec3 dir, float max_dist, out Hit hit, bool sunray) {
	// make ray relative to world texture and bring coordinates into [1.0, 2.0] float space
	// for float mantissa optimization (avoid int -> float conversion for projection)
	// basially treat float mantissa like integer for stepping but use the whole float for projection calculations
	vec3 coord = pos * INV_WORLD_SIZEf + 1.0; // to [1.0, 2.0]
	coord = clamp(coord, 1.0, 2.0);
	
	// flip coordinate space such that ray is always positive (simplifies stepping logic)
	// keep track of flip via flipmask
	bvec3 ray_neg = lessThan(dir, vec3(0.0));
	coord = mix(coord, 3.0 - coord, ray_neg);
	
	uvec3 flipmask = mix(uvec3(0u), uvec3(MANTISSA_MASK), ray_neg);
	
	// precompute part of plane projection equation
	// prefer  'pos * inv_dir + bias'  over  'inv_dir * (pos - ray_pos)'
	// due to mad instruction
	// multiply in WORLD_SIZEf to make distances be in world space
	vec3 inv_dir = mix(1.0 / abs(dir), vec3(INF), equal(dir, vec3(0.0))) * WORLD_SIZEf;
	vec3 bias = inv_dir * -coord;
	
	// starting cell is where ray is
	
	// start at some level of octree
	// -best to start at 0 if camera on surface
	// -best at higher levels if camera were in a large empty region
	uint mip = 0;
	//uint mip = uint(OCTREE_MIPS-1);
	
	// round down to start cell of octree
	coord = TOFLOAT(TOUINT(coord) & (ROUND_MASK << mip));
	
	float dist = 0.0;
	
	for (;;) {
		#if VISUALIZE_COST
		++iterations;
		#if VISUALIZE_WARP_COST
		if (subgroupElect()) atomicAdd(warp_iter[gl_SubgroupID], 1u);
		#endif
		#endif
	
		#if 1 // faster in my case
		// get current original coordinate space
		int bits = OCTREE_MIPS - int(mip); // 1 sub
		int offs = (MANTISSA_BITS - OCTREE_MIPS) + int(mip); // 1 add
		uvec3 flipped = bitfieldExtract(TOUINT(coord) ^ flipmask, offs, bits); // 3 xor 3 bfe
		
		// read octree cell
		uint childmask = texelFetch(octree, ivec3(flipped >> 1u), int(mip)).r; // 3 rshft
		
		uint i = flipped.x & 1u; // 1 and
		i = bitfieldInsert(i, flipped.y, 1, 1); // 1 bfe
		i = bitfieldInsert(i, flipped.z, 2, 1); // 1 bfe
		#else
		uvec3 flipped = TOUINT(coord) ^ flipmask; // 3 xors
		
		int bits = (OCTREE_MIPS-1) - int(mip); // 1 sub
		int offs = (MANTISSA_BITS - OCTREE_MIPS) + int(mip); // 1 add
		uvec3 texcoord = bitfieldExtract(flipped, offs + 1, bits); // 1 add + 3 BFE
		uvec3 bitcoord = bitfieldExtract(flipped, offs, 1); // 3 BFE
		
		uint childmask = texelFetch(octree, ivec3(texcoord), int(mip)).r;
		uint i = (bitcoord.z << 2) | ((bitcoord.y << 1) | bitcoord.x); // 2 mad or 2 LSHL_OR best case
		#endif
		
		if ((childmask & (1u << i)) != 0) {
			// non-air octree cell
			if (mip == 0u)
				break; // found solid leaf voxel
			
			// decend octree
			mip--;
			vec3 next_coord = TOFLOAT(TOUINT(coord) + (VOXEL_SIZE << mip));
			
			// upate coord by determining which child octant is entered first
			// by comparing ray hit against middle plane hits
			vec3 tmidv = inv_dir * next_coord + bias;
			
			coord = mix(coord, next_coord, lessThan(tmidv, vec3(dist)));
			
		} else {
			// air octree cell, continue stepping
			vec3 next_coord = TOFLOAT(TOUINT(coord) + (VOXEL_SIZE << mip));
			
			// calculate entry distances of next octree cell
			vec3 t0v = inv_dir * next_coord + bias;
			dist = min(min(t0v.x, t0v.y), t0v.z);
			
			#if 0
			// step into next cell via relevant axis
			bvec3 axismask;
			axismask.x = t0v.x == dist;
			axismask.y = t0v.y == dist && !axismask.x;
			axismask.z = !axismask.x && !axismask.y;
			
			coord = mix(coord, next_coord, axismask);
			
			uint stepcoord = axismask.x ? TOUINT(coord.x) : TOUINT(coord.z);
			stepcoord = axismask.y ? TOUINT(coord.y) : stepcoord;
			
			mip = findLSB(stepcoord) - MANTISSA_SHIFT;
			
			coord = TOFLOAT(TOUINT(coord) & (ROUND_MASK << mip));
			#else
			float stepcoord;
			if (t0v.x == dist) {
				coord.x = next_coord.x;
				stepcoord = coord.x;
			} else if (t0v.y == dist) {
				coord.y = next_coord.y;
				stepcoord = coord.y;
			} else {
				coord.z = next_coord.z;
				stepcoord = coord.z;
			}
			mip = findLSB(TOUINT(stepcoord)) - MANTISSA_SHIFT;
			coord = TOFLOAT(TOUINT(coord) & (ROUND_MASK << mip));
			#endif
			
			if (mip >= uint(OCTREE_MIPS) || dist >= max_dist)
				return false;
		}
	}
	
	if (!sunray) {
		int bits = OCTREE_MIPS - int(mip);
		int offs = (MANTISSA_BITS - OCTREE_MIPS) + int(mip);
		uvec3 flipped = bitfieldExtract(TOUINT(coord) ^ flipmask, offs, bits);
		
		// arrived at solid leaf voxel, read block id from seperate data structure
		hit.bid = read_bid(flipped);
		hit.prev_bid = 0; // don't know, could read
		
		// calcualte surface hit info
		hit.dist = dist;
		hit.pos = pos + dir * dist;
		
		vec2 uv;
		int face;
		{ // calc hit face, uv and normal
			vec3 hit_fract = fract(hit.pos);
			vec3 hit_center = vec3(flipped) + 0.5;
			
			vec3 offs = (hit.pos - hit_center);
			vec3 abs_offs = abs(offs);
			
			hit.normal = vec3(0.0);
			
			if (abs_offs.x >= abs_offs.y && abs_offs.x >= abs_offs.z) {
				hit.normal.x = sign(offs.x);
				face = offs.x < 0.0 ? 0 : 1;
				uv = hit_fract.yz;
				if (offs.x < 0.0) uv.x = 1.0 - uv.x;
			} else if (abs_offs.y >= abs_offs.z) {
				hit.normal.y = sign(offs.y);
				face = offs.y < 0.0 ? 2 : 3;
				uv = hit_fract.xz;
				if (offs.y >= 0.0) uv.x = 1.0 - uv.x;
			} else {
				hit.normal.z = sign(offs.z);
				face = offs.z < 0.0 ? 4 : 5;
				uv = hit_fract.xy;
				if (offs.z < 0.0) uv.y = 1.0 - uv.y;
			}
		}
		
		float texid = float(block_tiles[hit.bid].sides[face]);
		
		hit.col = texture(tile_textures, vec3(uv, texid)).rgb;
		hit.emiss = hit.col * get_emmisive(hit.bid);
	}
	return true;
}

#if !ONLY_PRIMARY_RAYS
float fresnel (vec3 view, vec3 norm, float F0) {
	float x = clamp(1.0 - dot(view, norm), 0.0, 1.0);
	float x2 = x*x;
	return F0 + ((1.0 - F0) * x2 * x2 * x);
}

vec3 hemisphere_sample () {
	// cosine weighted sampling (100% diffuse)
	// http://www.rorydriscoll.com/2009/01/07/better-sampling/
	
	// takes a uniform sample on a disc (x,y)
	// and projects into up into a hemisphere to get the cosine weighted points on the hemisphere
	
	// random sampling (Monte Carlo)
	vec2 uv = rand2(); // uniform sample in [0,1) square
	
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

// TODO: could optimize this to be precalculated for all normals, since we currently only have 6
// this also will not really do what I want for arbitrary normals and also not work for normal mapping
// this just gives you a arbitrary tangent and bitangent that does not correspond to the uvs at all
mat3 get_tangent_to_world (vec3 normal) {
	vec3 tangent = abs(normal.x) >= 0.9 ? vec3(0,1,0) : vec3(1,0,0);
	vec3 bitangent = cross(normal, tangent);
	tangent = cross(bitangent, normal);
	
	return mat3(tangent, bitangent, normal);
}

uniform bool  sunlight_enable = true;
uniform float sunlight_dist = 90.0;
uniform vec3  sunlight_col = vec3(0.98, 0.92, 0.65) * 1.3;

uniform vec3  ambient_light;

uniform bool  bounces_enable = true;
uniform float bounces_max_dist = 30.0;
uniform int   bounces_max_count = 8;

uniform bool  visualize_light = false;

uniform vec3 sun_pos = vec3(-28, 67, 102);
uniform float sun_pos_size = 4.0;

uniform vec3 sun_dir = normalize(vec3(-1,2,3));
uniform float sun_dir_rand = 0.05;

uniform float water_F0 = 0.6;

const float water_IOR = 1.333;
const float air_IOR = 1.0;

uniform sampler2D water_normal;

uniform float water_normal_time = 0.0; // wrap on some integer to avoid losing precision over time
uniform float water_normal_scale = 0.1;
uniform float water_normal_strength = 0.05;

vec3 sample_water_normal (vec3 pos_world) {
	vec2 uv1 = (pos_world.xy + water_normal_time * 0.2) * water_normal_scale;
	vec2 uv2 = (pos_world.xy + -water_normal_time * 0.2) * water_normal_scale * 0.5;
	uv2.xy = uv2.yx;
	
	vec2 a = texture(water_normal, uv1).rg * 2.0 - 1.0;
	vec2 b = texture(water_normal, uv2).rg * 2.0 - 1.0;
	//
	return normalize(vec3((a+b) * water_normal_strength, 1.0));
}

bool trace_ray_refl_refr (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit, out bool was_reflected) {
#if 0
	for (int j=0; j<2; ++j) {
		if (!trace_ray(ray_pos, ray_dir, max_dist, hit))
			break;
		
		//bool water = hit.bid == B_WATER || hit.prev_bid == B_WATER;
		//bool air = hit.bid == B_AIR || hit.prev_bid == B_AIR;
		//bool water_surface = water && air;
		
		bool water_surface = hit.bid == B_WATER;
		
		if (!water_surface)
			return true;
		
		if (hit.normal.z > 0.0)
			hit.normal = sample_water_normal(hit.pos);
		
		float reflect_fac = fresnel(-ray_dir, hit.normal, water_F0);
		
		float eta = hit.bid == B_WATER ? air_IOR / water_IOR : water_IOR / air_IOR;
		
		vec3 reflect_dir = reflect(ray_dir, hit.normal);
		vec3 refract_dir = refract(ray_dir, hit.normal, eta);
		
		if (dot(refract_dir, refract_dir) == 0.0) {
			// total internal reflection, ie. outside of snells window
			reflect_fac = 1.0;
		}
		
		if (rand() <= reflect_fac) {
			// reflect
			ray_pos = hit.pos + hit.normal * 0.001;
			ray_dir = reflect_dir;
		} else {
			// refract
			ray_pos = hit.pos + hit.normal * 0.001;
			ray_dir = refract_dir;
		}
		max_dist -= hit.dist;
	}
	
	return false;
#else
	bool did_hit = trace_ray(ray_pos, ray_dir, max_dist, hit, false);

#if REFLECTIONS
	if (did_hit && hit.bid == B_WATER) {
		// reflect
		
		vec3 normal_map = hit.normal;
		
		#ifndef RT_LIGHT
		if (hit.normal.z > 0.0)
			normal_map = sample_water_normal(hit.pos);
		#endif
		
		ray_pos = hit.pos + normal_map * 0.001;
		ray_dir = reflect(ray_dir, normal_map);
		max_dist -= hit.dist;
		
		if (dot(ray_dir, hit.normal) < 0.0) {
			hit.col = vec3(0,0,0); // can't reflect below water (normal_map vector was
			return true;
		}
		
		was_reflected = true;
		return trace_ray(ray_pos, ray_dir, max_dist, hit, false);
	}
#endif
	was_reflected = false;
	return did_hit;
#endif
}

vec3 collect_sunlight (vec3 pos, vec3 normal) {
	if (sunlight_enable) {
		#if SUNLIGHT_MODE == 0
		// directional sun
		vec3 dir = sun_dir + rand3()*sun_dir_rand;
		float cos = dot(dir, normal);
		
		Hit hit;
		if (cos > 0.0 && !trace_ray(pos, dir, sunlight_dist, hit, true))
			return sunlight_col * cos;
		#else
		// point sun
		vec3 offs = (sun_pos + (rand3()-0.5) * sun_pos_size) - pos;
		float dist = length(offs);
		vec3 dir = normalize(offs);
		
		float cos = dot(dir, normal);
		float atten = 16000.0 / (dist*dist);
		
		float max_dist = dist - sun_pos_size*0.5;
		
		Hit hit;
		if (cos > 0.0 && !trace_ray(pos, dir, max_dist, hit, true))
			return sunlight_col * (cos * atten);
		#endif
	}
	return vec3(0.0);
}
#endif
