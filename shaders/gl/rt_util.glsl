
#include "common.glsl"
#include "rand.glsl"

#define PI	3.1415926535897932384626433832795
#define INF (1. / 0.)

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

uniform usampler3D	voxels[2];

#define WORLD_SIZE			16 // number of chunks for fixed subchunk texture (for now)

#define TEX3D_SIZE			2048 // max width, height, depth for 3d textures

#define SUBCHUNK_TEX_COUNT	(TEX3D_SIZE / SUBCHUNK_SIZE) // max num of subchunks in one axis for tex
#define SUBCHUNK_TEX_SHIFT	8
#define SUBCHUNK_TEX_MASK	((SUBCHUNK_TEX_COUNT-1) << SUBCHUNK_SHIFT)

#define CHUNK_OCTREE_LAYERS  CHUNK_SIZE_SHIFT

uniform usampler3D	octree;

// subchunk id to 3d tex offset (including subchunk_size multiplication)
ivec3 subchunk_id_to_texcoords (uint id) {
	ivec3 coord;
	coord.x = int((id << (SUBCHUNK_TEX_SHIFT*0 + SUBCHUNK_SHIFT)) & SUBCHUNK_TEX_MASK);
	coord.y = int((id >> (SUBCHUNK_TEX_SHIFT*1 - SUBCHUNK_SHIFT)) & SUBCHUNK_TEX_MASK);
	coord.z = int((id >> (SUBCHUNK_TEX_SHIFT*2 - SUBCHUNK_SHIFT)) & SUBCHUNK_TEX_MASK);
	return coord;
}

uniform int max_iterations = 200;

#if VISUALIZE_COST
uniform sampler2D		heat_gradient;
	#if VISUALIZE_WARP_COST
		#define WARP_COUNT_IN_WG ((LOCAL_SIZE_X*LOCAL_SIZE_Y) / 32) 
		shared uint warp_iter[WARP_COUNT_IN_WG];
	#endif
#endif

uniform bool  sunlight_enable = true;
uniform float sunlight_dist = 90.0;
uniform vec3  sunlight_col = vec3(0.98, 0.92, 0.65) * 1.3;

uniform vec3  ambient_light;

uniform bool  bounces_enable = true;
uniform float bounces_max_dist = 30.0;
uniform int   bounces_max_count = 8;

uniform int   rays = 1;

uniform bool  visualize_light = false;

uniform vec3 sun_pos = vec3(-28, 67, 102);
uniform float sun_pos_size = 4.0;

uniform vec3 sun_dir = normalize(vec3(-1,2,3));
uniform float sun_dir_rand = 0.05;

uniform float water_F0 = 0.6;

const float water_IOR = 1.333;
const float air_IOR = 1.0;

//
struct Ray {
	vec3	pos;
	vec3	dir;
	float	max_dist;
};

struct Hit {
	vec3	pos;
	vec3	normal;
	float	dist;
	uint	bid;
	uint	prev_bid;
	vec3	col;
	vec3	emiss;
};

int iterations = 0;

int get_step_face (int axis, ivec3 flipmask) {
	if (axis == 0)		return flipmask.x == 0 ? 0 : 1;
	else if (axis == 1)	return flipmask.y == 0 ? 2 : 3;
	else				return flipmask.z == 0 ? 4 : 5;
}
vec2 calc_uv (vec3 pos_fract, int axis, int entry_face) {
	vec2 uv;
	if (axis == 0) {
		uv = pos_fract.yz;
	} else if (axis == 1) {
		uv = pos_fract.xz;
	} else {
		uv = pos_fract.xy;
	}

	if (entry_face == 0 || entry_face == 3)  uv.x = 1.0 - uv.x;
	if (entry_face == 4)                     uv.y = 1.0 - uv.y;

	return uv;
}

bool _dbg_ray = false;
uniform bool update_debug_rays = false;

#if 0 // sparse voxel storage raytracer
bool hit_voxel (uint bid, uint prev_bid, int axis, float dist,
		vec3 ray_pos, vec3 ray_dir, ivec3 flipmask, out Hit hit) {
	if (prev_bid == 0 || (bid == prev_bid && (bid != B_LEAVES && bid != B_TALLGRASS)))
		return false;
	
	if (bid == B_AIR)
		return false;
	
	vec3 hit_pos = (ray_pos + ray_dir * dist) * mix(vec3(-1), vec3(1), equal(flipmask, ivec3(0.0)));
	
	int entry_face = get_step_face(axis, flipmask);
	vec2 uv = calc_uv(fract(hit_pos), axis, entry_face);
	
	#ifdef RT_LIGHT
	if (bid == B_AIR)
		return false;
	uint tex_bid = bid;
	#else
	uint tex_bid = bid == B_AIR ? prev_bid : bid;
	#endif
	float texid = float(block_tiles[tex_bid].sides[entry_face]);
	
	//vec4 col = textureLod(tile_textures, vec3(uv, texid), log2(dist) - 5.8);
	vec4 col = texture(tile_textures, vec3(uv, texid));
	
	//if (tex_bid == B_TALLGRASS && axis == 2)
	//	col = vec4(0.0);
	
	col.a = 1;
	//if (col.a <= 0.001)
	//	return false;
	
	hit.pos = hit_pos;
	hit.normal = mix(vec3(0.0), mix(ivec3(+1), ivec3(-1), equal(flipmask, ivec3(0))), equal(ivec3(axis), ivec3(0,1,2)));
	
	hit.bid = bid;
	hit.prev_bid = prev_bid;
	
	hit.dist = dist;
	hit.col = col.rgb;
	hit.emiss = col.rgb * get_emmisive(hit.bid);
	return true;
}

void hit_voxel_sun (uint bid, uint prev_bid, int axis, float dist,
		vec3 ray_pos, vec3 ray_dir, ivec3 flipmask, inout float alpha) {
	if (prev_bid == 0 || (bid == prev_bid && (bid != B_LEAVES && bid != B_TALLGRASS)))
		return;
	
	if (bid == B_AIR)
		return;
	
	alpha = 0.0;
	return;
	
	vec3 hit_pos = (ray_pos + ray_dir * dist) * mix(vec3(-1), vec3(1), equal(flipmask, ivec3(0.0)));
	
	int entry_face = get_step_face(axis, flipmask);
	vec2 uv = calc_uv(fract(hit_pos), axis, entry_face);
	
	#ifdef RT_LIGHT
	if (bid == B_AIR)
		return;
	uint tex_bid = bid;
	#else
	uint tex_bid = bid == B_AIR ? prev_bid : bid;
	#endif
	float texid = float(block_tiles[tex_bid].sides[entry_face]);
	
	float a = textureLod(tile_textures, vec3(uv, texid), 20.0).a;
	
	if (tex_bid == B_TALLGRASS && axis == 2)
		a = 0.0;
	
	if (a <= 0.001)
		return;
	
	alpha -= a * alpha;
}

bool _trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit) {
	
	ivec3 flipmask = mix(ivec3(0), ivec3(-1), lessThan(ray_dir, vec3(0.0)));
	ray_pos       *= mix(vec3(1), vec3(-1), lessThan(ray_dir, vec3(0.0)));
	
	ray_dir = abs(ray_dir);
	
	vec3 rdir = mix(1.0 / ray_dir, vec3(INF), equal(ray_dir, vec3(0.0)));
	ivec3 coord = ivec3(floor(ray_pos / float(SUBCHUNK_SIZE))) * SUBCHUNK_SIZE;
	
	float dist = 0; 
	int axis;
	
	uint prev_bid = 0;
	uint bid = 0;
	
	uint subchunk;
	int new_coord = 0;
	
	for (;;) {
	#if VISUALIZE_COST
		++iterations;
		#if VISUALIZE_WARP_COST
			if (subgroupElect())
				atomicAdd(warp_iter[gl_SubgroupID], 1u);
		#endif
	#endif
		if (dist >= max_dist)
			return false; // max dist reached
		
		if ((new_coord & SUBCHUNK_MASK) == 0) {
			coord &= ~SUBCHUNK_MASK;
			
			ivec3 scoord = (coord ^ flipmask) + (WORLD_SIZE/2) * CHUNK_SIZE;
			
			if (!all(lessThan(uvec3(scoord), uvec3(WORLD_SIZE * CHUNK_SIZE))))
				return false;
			
			subchunk = texelFetch(voxels[0], scoord >> SUBCHUNK_SHIFT, 0).r;
		}
		
		int stepsize;
		if ((subchunk & SUBC_SPARSE_BIT) != 0) {
			stepsize = SUBCHUNK_SIZE;
			
			bid = subchunk & ~SUBC_SPARSE_BIT;
			
			if (bid == 0)
				return false; // unloaded chunk
		} else {
			if ((new_coord & SUBCHUNK_MASK) == 0) {
				vec3 proj = ray_pos + ray_dir * dist;
				coord = clamp(ivec3(floor(proj)), coord, coord + ivec3(SUBCHUNK_SIZE -1));
			}
			stepsize = 1;
			
			ivec3 subc_offs = subchunk_id_to_texcoords(subchunk);
			bid = texelFetch(voxels[1], subc_offs + ((coord ^ flipmask) & SUBCHUNK_MASK), 0).r;
		}
		
		if (hit_voxel(bid, prev_bid, axis, dist, ray_pos, ray_dir, flipmask, hit))
			return true;
		prev_bid = bid;
		
		vec3 next = rdir * (vec3(coord + stepsize) - ray_pos);
		
		dist = min(min(next.x, next.y), next.z);
		
		if (next.x == dist) {
			axis = 0;
			
			coord.x += stepsize;
			new_coord = coord.x;
		} else if (next.y == dist) {
			axis = 1;
			
			coord.y += stepsize;
			new_coord = coord.y;
		} else {
			axis = 2;
			
			coord.z += stepsize;
			new_coord = coord.z;
		}
	}
}

float _trace_sunray (vec3 ray_pos, vec3 ray_dir, float max_dist, out float hit_dist) {
	
	ivec3 flipmask = mix(ivec3(0), ivec3(-1), lessThan(ray_dir, vec3(0.0)));
	ray_pos       *= mix(vec3(1), vec3(-1), lessThan(ray_dir, vec3(0.0)));
	
	ray_dir = abs(ray_dir);
	
	vec3 rdir = mix(1.0 / ray_dir, vec3(INF), equal(ray_dir, vec3(0.0)));
	ivec3 coord = ivec3(floor(ray_pos / float(SUBCHUNK_SIZE))) * SUBCHUNK_SIZE;
	
	float dist = 0; 
	int axis;
	
	uint prev_bid = 0;
	uint bid = 0;
	
	uint subchunk;
	int new_coord = 0;
	
	float alpha = 1.0;
	
	for (;;) {
		if ((new_coord & SUBCHUNK_MASK) == 0) {
			coord &= ~SUBCHUNK_MASK;
			
			ivec3 scoord = (coord ^ flipmask) + (WORLD_SIZE/2) * CHUNK_SIZE;
			
			if (!all(lessThan(uvec3(scoord), uvec3(WORLD_SIZE * CHUNK_SIZE))))
				break;
			
			subchunk = texelFetch(voxels[0], scoord >> SUBCHUNK_SHIFT, 0).r;
		}
		
		int stepsize;
		if ((subchunk & SUBC_SPARSE_BIT) != 0) {
			stepsize = SUBCHUNK_SIZE;
			
			bid = subchunk & ~SUBC_SPARSE_BIT;
			
			if (bid == 0)
				break; // unloaded chunk
		} else {
			if ((new_coord & SUBCHUNK_MASK) == 0) {
				vec3 proj = ray_pos + ray_dir * dist;
				coord = clamp(ivec3(floor(proj)), coord, coord + ivec3(SUBCHUNK_SIZE -1));
			}
			stepsize = 1;
			
			ivec3 subc_offs = subchunk_id_to_texcoords(subchunk);
			bid = texelFetch(voxels[1], subc_offs + ((coord ^ flipmask) & SUBCHUNK_MASK), 0).r;
		}
		
		hit_voxel_sun(bid, prev_bid, axis, dist, ray_pos, ray_dir, flipmask, alpha);
		if (alpha <= 0.001)
			break;
		prev_bid = bid;
		
		vec3 next = rdir * (vec3(coord + stepsize) - ray_pos);
		
		dist = min(min(next.x, next.y), next.z);
		
	#if VISUALIZE_COST && VISUALIZE_WARP_COST
		if (subgroupElect())
			atomicAdd(warp_iter[gl_SubgroupID], 1u);
	#endif
		if (dist >= max_dist)
			break; // max dist reached
		
		if (next.x == dist) {
			axis = 0;
			
			coord.x += stepsize;
			new_coord = coord.x;
		} else if (next.y == dist) {
			axis = 1;
			
			coord.y += stepsize;
			new_coord = coord.y;
		} else {
			axis = 2;
			
			coord.z += stepsize;
			new_coord = coord.z;
		}
	}
	
	hit_dist = dist;
	return max(alpha, 0.0);
}
#else // Octree raytracer
uint read_bid (ivec3 coord) {
	ivec3 scoord = (coord & ~SUBCHUNK_MASK);
	if (!all(lessThan(uvec3(scoord), uvec3(WORLD_SIZE * CHUNK_SIZE))))
		return 0;
	
	uint subchunk = texelFetch(voxels[0], scoord >> SUBCHUNK_SHIFT, 0).r;
	
	if ((subchunk & SUBC_SPARSE_BIT) != 0) {
		return subchunk & ~SUBC_SPARSE_BIT;
	} else {
		ivec3 subc_offs = subchunk_id_to_texcoords(subchunk);
		return texelFetch(voxels[1], subc_offs + (coord & SUBCHUNK_MASK), 0).r;
	}
}

bool __trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit, bool sunray) {
	
	// make ray relative to world texture
	ray_pos += float(WORLD_SIZE/2 * CHUNK_SIZE);
	
	// flip coordinate space for ray such that ray dir is all positive
	// keep track of this flip via flipmask
	ivec3 flipmask = mix(ivec3(0), ivec3(-1), lessThan(ray_dir, vec3(0.0)));
	ray_pos       *= mix(vec3(1), vec3(-1), lessThan(ray_dir, vec3(0.0)));
	
	ray_dir = abs(ray_dir);
	
	// precompute part of plane projection equation
	vec3 rdir = mix(1.0 / ray_dir, vec3(INF), equal(ray_dir, vec3(0.0)));
	
	// starting cell is where ray is
	ivec3 coord = ivec3(floor(ray_pos));
	
	int axis = 0;
	float t0;
	
	// start at highest level of octree
	int mip = CHUNK_OCTREE_LAYERS-1;
	
	for (;;) {
		
	#if VISUALIZE_COST
		++iterations;
		#if VISUALIZE_WARP_COST
			if (subgroupElect())
				atomicAdd(warp_iter[gl_SubgroupID], 1u);
		#endif
	#endif
		
		// get octree cell size of current octree level
		int size = 1 << mip;
		coord &= ~(size-1); // coord = bitfieldInsert(coord, ivec3(0), 0, mip);
		
		// calculate both entry and exit distances of current octree cell
		vec3 t0v = rdir * (vec3(coord       ) - ray_pos);
		vec3 t1v = rdir * (vec3(coord + size) - ray_pos);
		t0 = max(max(t0v.x, t0v.y), t0v.z);
		float t1 = min(min(t1v.x, t1v.y), t1v.z);
		
		// handle rays starting in a cell (hit at distance 0)
		t0 = max(t0, 0.0);
		
		bool vox;
		{
			// flip coord back into original coordinate space
			ivec3 flipped = (coord ^ flipmask);
			
			// handle both stepping out of 3d texture and reaching max ray distance
			if ( !all(lessThan(uvec3(flipped), uvec3(WORLD_SIZE * CHUNK_SIZE))) ||
				 t1 >= max_dist )
				return false;
			
			// read octree cell
			flipped >>= mip;
			uint childmask = texelFetch(octree, flipped >> 1, mip).r;
			
			//int i = (flipped.x&1) | ((flipped.y&1) << 1) | ((flipped.z&1) << 2);
			int i = flipped.x & 1;
			i = bitfieldInsert(i, flipped.y, 1, 1);
			i = bitfieldInsert(i, flipped.z, 2, 1);
			
			vox = (childmask & (1u << i)) != 0;
		}
		
		if (vox) {
			// non-air octree cell
			if (mip == 0) 
				break; // found solid leaf voxel
			
			// decend octree
			mip--;
			ivec3 child_size = ivec3(1 << mip);
			
			// upate coord by determining which child octant is entered first
			// by comparing ray hit against middle plane hits
			vec3 tmidv = rdir * (vec3(coord + child_size) - ray_pos);
			
			coord = mix(coord, coord + child_size, lessThan(tmidv, vec3(t0)));
			
		} else {
			// air octree cell, continue stepping
			
		#if 1 // better performance
		
			// step into next cell via relevant axis
			int stepbit;
			if (t1v.x == t1) {
				axis = 0;
				
				int old = coord.x;
				coord.x += size;
				// determine which bit has changed during increment
				stepbit = coord.x ^ old;
				
			} else if (t1v.y == t1) {
				axis = 1;
				
				int old = coord.y;
				coord.y += size;
				stepbit = coord.y ^ old;
			} else {
				axis = 2;
				
				int old = coord.z;
				coord.z += size;
				stepbit = coord.z ^ old;
			}
		#else
			bvec3 axismask = equal(t1v, vec3(t1));
			
			ivec3 old = coord;
			coord = mix(coord, coord + size, axismask);
			
			ivec3 stepbits = old ^ coord;
			int stepbit = stepbits.x | stepbits.y | stepbits.z;
			
			ivec3 masked = mix(ivec3(0), ivec3(0,1,2), axismask);
			axis = masked.x + masked.y + masked.z;
		#endif
			
			// determine highest changed octree parent by scanning for MSB that was changed
			mip = min(findMSB(uint(stepbit)), CHUNK_OCTREE_LAYERS-1);
		}
	}
	
	if (!sunray) {
		// arrived at solid leaf voxel, read block id from seperate data structure
		uint bid = read_bid(coord ^ flipmask);
		
		{ // calcualte surface hit info
			vec3 hit_pos = (ray_pos + ray_dir * t0) * mix(vec3(-1), vec3(1), equal(flipmask, ivec3(0.0)));
			// make ray not relative to world texture again
			hit_pos -= float(WORLD_SIZE/2 * CHUNK_SIZE);
		
			int entry_face = get_step_face(axis, flipmask);
			vec2 uv = calc_uv(fract(hit_pos), axis, entry_face);
			
			float texid = float(block_tiles[bid].sides[entry_face]);
			
			vec4 col = texture(tile_textures, vec3(uv, texid));
			col.a = 1;
			
			hit.pos = hit_pos;
			hit.normal = mix(vec3(0.0), mix(ivec3(+1), ivec3(-1), equal(flipmask, ivec3(0))), equal(ivec3(axis), ivec3(0,1,2)));
			
			hit.bid = bid;
			hit.prev_bid = 0; // don't know, could read
			
			hit.dist = t0;
			hit.col = col.rgb;
			hit.emiss = col.rgb * get_emmisive(hit.bid);
		}
	}
	return true;
}

bool _trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit) {
	return __trace_ray(ray_pos, ray_dir, max_dist, hit, false);
}
float _trace_sunray (vec3 ray_pos, vec3 ray_dir, float max_dist, out float hit_dist) {
	Hit hit;
	if (__trace_ray(ray_pos, ray_dir, max_dist, hit, true)) {
		hit_dist = hit.dist;
		return 0.0;
	} else {
		hit_dist = max_dist;
		return 1.0;
	}
}

#endif

bool trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit) {
	bool did_hit = _trace_ray(ray_pos, ray_dir, max_dist, hit);
	if (_dbg_ray) dbg_draw_vector(ray_pos, ray_dir*(did_hit ? hit.dist : min(max_dist, 5000.0)), vec4(1,0,0,1));
	return did_hit;
}
float trace_sunray (vec3 ray_pos, vec3 ray_dir, float max_dist) {
	float dist;
	float alpha = _trace_sunray(ray_pos, ray_dir, max_dist, dist);
	if (_dbg_ray) dbg_draw_vector(ray_pos, ray_dir*dist, vec4(1,1,0,1));
	return alpha;
}

#if !ONLY_PRIMARY_RAYS
bool trace_ray_refl_refr (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit) {
	for (int j=0; j<2; ++j) {
		if (!trace_ray(ray_pos, ray_dir, max_dist, hit))
			break;
		
		//bool water = hit.bid == B_WATER || hit.prev_bid == B_WATER;
		//bool air = hit.bid == B_AIR || hit.prev_bid == B_AIR;
		//bool water_surface = water && air;
		
		bool water_surface = hit.bid == B_WATER;
		
		if (!water_surface)
			return true;
		
		//col += hit.col * light * 0.1;
		
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
}

vec3 collect_sunlight (vec3 pos, vec3 normal) {
	if (sunlight_enable) {
		#if SUNLIGHT_MODE == 0
		// directional sun
		vec3 dir = sun_dir + rand3()*sun_dir_rand;
		float cos = dot(dir, normal);
		
		if (cos > 0.0) {
			float alpha = trace_sunray(pos, dir, sunlight_dist);
			return sunlight_col * (cos * alpha);
		}
		#else
		// point sun
		vec3 offs = (sun_pos + (rand3()-0.5) * sun_pos_size) - pos;
		float dist = length(offs);
		vec3 dir = normalize(offs);
		
		float cos = dot(dir, normal);
		float atten = 16000.0 / (dist*dist);
		
		if (cos > 0.0) {
			float max_dist = dist - sun_pos_size*0.5;
			float alpha = trace_sunray(pos, dir, max_dist);
			return sunlight_col * (cos * atten * alpha);
		}
		#endif
	}
	return vec3(0.0);
}
#endif
