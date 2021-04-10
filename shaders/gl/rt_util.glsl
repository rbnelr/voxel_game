
#include "common.glsl"
#include "rand.glsl"

#define PI	3.1415926535897932384626433832795
#define INF (1. / 0.)

uniform usampler3D	voxels[2];

#define WORLD_SIZE			16 // number of chunks for fixed subchunk texture (for now)

#define TEX3D_SIZE			2048 // max width, height, depth for 3d textures
#define TEX3D_SIZE_SHIFT	11

#define SUBCHUNK_TEX_SHIFT	(TEX3D_SIZE_SHIFT - SUBCHUNK_SHIFT)

#define CHUNK_OCTREE_LAYERS  CHUNK_SIZE_SHIFT

uniform usampler3D	octree;

#if VISUALIZE_COST
int iterations = 0;

uniform int max_iterations = 200;

uniform sampler2D		heat_gradient;
	#if VISUALIZE_WARP_COST
		#define WARP_COUNT_IN_WG ((LOCAL_SIZE_X*LOCAL_SIZE_Y) / 32) 
		shared uint warp_iter[WARP_COUNT_IN_WG];
	#endif
#endif

#define DEBUG_RAYS 1
#if DEBUG_RAYS
bool _dbg_ray = false;
uniform bool update_debug_rays = false;
#endif

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

uint get_step_face (bvec3 axismask, ivec3 flipmask) {
	if (axismask.x)      return flipmask.x == 0 ? 0 : 1;
	else if (axismask.y) return flipmask.y == 0 ? 2 : 3;
	else                 return flipmask.z == 0 ? 4 : 5;
}
vec2 calc_uv (vec3 pos_fract, bvec3 axismask, uint entry_face) {
	vec2 uv;
	if (axismask.x) {
		uv = pos_fract.yz;
	} else if (axismask.y) {
		uv = pos_fract.xz;
	} else {
		uv = pos_fract.xy;
	}

	if (entry_face == 0u || entry_face == 3u)  uv.x = 1.0 - uv.x;
	if (entry_face == 4u)                      uv.y = 1.0 - uv.y;

	return uv;
}

uint read_bid (uvec3 coord) {
	uvec3 scoord = coord & ~SUBCHUNK_MASK;
	if (!all(lessThan(scoord, uvec3(WORLD_SIZE * CHUNK_SIZE))))
		return 0;
	
	uint subchunk = texelFetch(voxels[0], ivec3(scoord >> SUBCHUNK_SHIFT), 0).r;
	
	if ((subchunk & SUBC_SPARSE_BIT) != 0) {
		return subchunk & ~SUBC_SPARSE_BIT;
	} else {
		
		// subchunk id to 3d tex offset (including subchunk_size multiplication)
		// ie. split subchunk id into 3 sets of SUBCHUNK_TEX_SHIFT bits
		uvec3 subc_offs;
		subc_offs.x = bitfieldExtract(subchunk, SUBCHUNK_TEX_SHIFT*0, SUBCHUNK_TEX_SHIFT);
		subc_offs.y = bitfieldExtract(subchunk, SUBCHUNK_TEX_SHIFT*1, SUBCHUNK_TEX_SHIFT);
		subc_offs.z = bitfieldExtract(subchunk, SUBCHUNK_TEX_SHIFT*2, SUBCHUNK_TEX_SHIFT);
		
		// equivalent to  (coord & SUBCHUNK_MASK) | (subc_offs << SUBCHUNK_SHIFT)
		scoord = bitfieldInsert(coord, subc_offs, SUBCHUNK_SHIFT, 32 - SUBCHUNK_SHIFT);
		
		return texelFetch(voxels[1], ivec3(scoord), 0).r;
	}
}

bool trace_ray (vec3 pos, vec3 dir, float max_dist, out Hit hit, bool sunray) {
	
	// make ray relative to world texture
	vec3 ray_pos = pos + float(WORLD_SIZE/2 * CHUNK_SIZE);
	
	// flip coordinate space for ray such that ray dir is all positive
	// keep track of this flip via flipmask
	ivec3 flipmask = mix(ivec3(0u), ivec3(-1), lessThan(dir, vec3(0.0)));
	ray_pos       *= mix(vec3(1), vec3(-1), lessThan(dir, vec3(0.0)));
	
	
	// precompute part of plane projection equation
	vec3 abs_dir = abs(dir);
	vec3 rdir = mix(1.0 / abs_dir, vec3(INF), equal(abs_dir, vec3(0.0)));
	
	// starting cell is where ray is
	ivec3 coord = ivec3(floor(ray_pos));
	
	// start at highest level of octree
	uint mip = uint(CHUNK_OCTREE_LAYERS-1);
	
	bvec3 axismask = bvec3(false);
	float t0;
	bool did_hit;
	
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
		{ // read octree cell
			// flip coord back into original coordinate space
			uvec3 flipped = uvec3(coord ^ flipmask);
			
			// handle both stepping out of 3d texture and reaching max ray distance
			if ( !all(lessThan(flipped, uvec3(WORLD_SIZE * CHUNK_SIZE))) ||
				 t1 >= max_dist ) {
				did_hit = false;
				break;
			}
			
			// read octree cell
			flipped >>= mip;
			uint childmask = texelFetch(octree, ivec3(flipped >> 1u), int(mip)).r;
			
			//flipped &= 1u;
			//uint i = flipped.z*4u + (flipped.y*2u + flipped.x);
			uint i = flipped.x & 1u;
			i = bitfieldInsert(i, flipped.y, 1, 1);
			i = bitfieldInsert(i, flipped.z, 2, 1);
			
			vox = (childmask & (1u << i)) != 0;
			//vox = bitfieldExtract(childmask, int(i), 1) != 0; // slower
		}
		
		if (vox) {
			// non-air octree cell
			if (mip == 0u) {
				did_hit = true; // found solid leaf voxel
				break;
			}
			
			// decend octree
			mip--;
			int child_size = 1 << mip;
			
			// upate coord by determining which child octant is entered first
			// by comparing ray hit against middle plane hits
			vec3 tmidv = rdir * (vec3(coord + child_size) - ray_pos);
			
			coord += mix(ivec3(0), ivec3(child_size), lessThan(tmidv, vec3(t0)));
			
		} else {
			// air octree cell, continue stepping
			
			// step into next cell via relevant axis
			axismask = equal(t1v, vec3(t1));
			ivec3 old = coord;
			
			if (axismask.x)       coord.x += size;
			else if (axismask.y)  coord.y += size;
			else                  coord.z += size;
			
			// determine which bit has changed during increment
			int stepbit = (coord.x ^ old.x) | (coord.y ^ old.y) | (coord.z ^ old.z);
			
			// determine highest changed octree parent by scanning for MSB that was changed
			mip = min(findMSB(uint(stepbit)), uint(CHUNK_OCTREE_LAYERS-1));
		}
	}
	
	if (did_hit && !sunray) {
		// arrived at solid leaf voxel, read block id from seperate data structure
		uint bid = read_bid(uvec3(coord ^ flipmask));
		
		{ // calcualte surface hit info
			vec3 hit_pos = pos + dir * t0;
		
			uint entry_face = get_step_face(axismask, flipmask);
			vec2 uv = calc_uv(fract(hit_pos), axismask, entry_face);
			
			float texid = float(block_tiles[bid].sides[entry_face]);
			
			vec3 col = texture(tile_textures, vec3(uv, texid)).rgb;
			
			hit.pos = hit_pos;
			hit.normal = mix(vec3(0.0), -sign(dir), axismask);
			
			hit.bid = bid;
			hit.prev_bid = 0; // don't know, could read
			
			hit.dist = t0;
			hit.col = col.rgb;
			hit.emiss = col.rgb * get_emmisive(hit.bid);
		}
	}
	
	#if DEBUG_RAYS
	if (_dbg_ray) dbg_draw_vector(pos, dir * t0, sunray ? vec4(1,1,0,1) : vec4(1,0,0,1));
	#endif
	
	return did_hit;
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
	if (did_hit && hit.bid == B_WATER) {
		// reflect
		ray_pos = hit.pos + hit.normal * 0.001;
		ray_dir = reflect(ray_dir, hit.normal);
		max_dist -= hit.dist;
		
		was_reflected = true;
		return trace_ray(ray_pos, ray_dir, max_dist, hit, false);
	}
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
