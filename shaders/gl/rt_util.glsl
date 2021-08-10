
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
		#define WARP_COUNT_IN_WG ((WG_PIXELS_X*WG_PIXELS_Y) / 32) 
		shared uint warp_iter[WARP_COUNT_IN_WG];
		
		#define INIT_VISUALIZE_COST \
			if (subgroupElect()) { \
				warp_iter[gl_SubgroupID] = 0u; \
			} \
			iterations = 0; \
			barrier();
			
		#define GET_VISUALIZE_COST(VAL) \
			const uint warp_cost = warp_iter[gl_SubgroupID]; \
			const uint local_cost = iterations; \
			 \
			float wasted_work = float(warp_cost - local_cost) / float(warp_cost); \
			VAL = texture(heat_gradient, vec2(wasted_work, 0.5)).rgb;
			
		#define VISUALIZE_COST_COUNT \
			++iterations; \
			if (subgroupElect()) atomicAdd(warp_iter[gl_SubgroupID], 1u);
	#else
		#define INIT_VISUALIZE_COST \
			iterations = 0;
			
		#define GET_VISUALIZE_COST(VAL) \
			VAL = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5)).rgb;
			
		#define VISUALIZE_COST_COUNT \
			++iterations;
	#endif
#else
	#define INIT_VISUALIZE_COST
	#define GET_VISUALIZE_COST(VAL)
	#define VISUALIZE_COST_COUNT
#endif

vec3 generate_tangent (vec3 normal) { // NOTE: tangents currently do not correspond with texture uvs, normal mapping will be wrong
	vec3 tangent = vec3(0,0,+1);
	if (abs(normal.z) > 0.99) return vec3(0,+1,0);
	return tangent;
}
mat3 calc_TBN (vec3 normal, vec3 tangent) {
	vec3 bitangent = cross(normal, tangent); // generate bitangent vector orthogonal to both normal and tangent
	tangent = cross(bitangent, normal); // regenerate tangent vector in case it was not orthogonal to normal
	
	return mat3(tangent, bitangent, normal);
}

// get pixel ray in world space based on pixel coord and matricies
bool get_ray (vec2 px_pos, out vec3 ray_pos, out vec3 ray_dir) {
	
#if 1 // Normal camera projection
	//vec2 px_center = px_pos + rand2();
	vec2 px_center = px_pos + vec2(0.5);
	vec2 ndc = px_center / view.viewport_size * 2.0 - 1.0;
	
	vec4 clip = vec4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;

	// TODO: can't get optimization to one single clip_to_world mat mul to work, why?
	// -> clip_to_cam needs translation  cam_to_world needs to _not_ have translation
	vec3 cam = (view.clip_to_cam * clip).xyz;

	ray_dir = (view.cam_to_world * vec4(cam, 0)).xyz;
	ray_dir = normalize(ray_dir);
	
	// ray starts on the near plane
	ray_pos = (view.cam_to_world * vec4(cam, 1)).xyz;
	
	// make relative to gpu world representation (could bake into matrix)
	ray_pos += float(WORLD_SIZE/2);
	
	return true;

#else // 360 Sphere Projections
	
	vec2 px_center = (px_pos + vec2(0.5)) / view.viewport_size; // [0,1]
	
	#if 0 // Equirectangular projection
		float lon = (px_center.x - 0.5) * PI*2;
		float lat = (px_center.y - 0.5) * PI;
	#else // Mollweide projection
		float x = px_center.x * 2.0 - 1.0;
		float y = px_center.y * 2.0 - 1.0;
		
		if ((x*x + y*y) > 1.0)
			return false;
		
		float theta = asin(y);
		
		float lon = (PI * x) / cos(theta);
		float lat = asin((2.0 * theta + sin(2.0 * theta)) / PI);
	#endif
	
	float c = cos(lat);
	vec3 dir_cam = vec3(c * sin(lon), sin(lat), -c * cos(lon));
	
	ray_dir = (view.cam_to_world * vec4(dir_cam, 0)).xyz;
	ray_pos = (view.cam_to_world * vec4(0,0,0,1)).xyz;
	
	// make relative to gpu world representation (could bake into matrix)
	ray_pos += float(WORLD_SIZE/2);

	return true;
#endif
}

#define RAYT_PRIMARY		0
#define RAYT_REFLECT		1
#define RAYT_DIFFUSE		2
#define RAYT_SPECULAR		3
#define RAYT_SUN			4

#define DEBUGDRAW 1
#if DEBUGDRAW
bool _dbgdraw_rays = false;
uniform bool update_debugdraw = false;

vec4 _dbg_ray_cols[] = {
	vec4(1,0,1,1),
	vec4(0,0,1,1),
	vec4(1,0,0,1),
	vec4(0,1,0,1),
	vec4(1,1,0,1),
};
#endif

//
struct Hit {
	vec3	pos;
	mat3	TBN;
	float	dist;
	uint	bid;
	uint	medium;
	vec3	col;
	vec2	occl_spec;
	float	emiss;
};

const float WORLD_SIZEf = float(WORLD_SIZE);
const float INV_WORLD_SIZEf = 1.0 / WORLD_SIZEf;
const uint ROUNDMASK = -1;
const uint FLIPMASK = WORLD_SIZE-1;

bool trace_ray (vec3 pos, vec3 dir, float max_dist, uint medium_bid, out Hit hit, int type) {
	// flip coordinate space such that ray is always positive (simplifies stepping logic)
	// keep track of flip via flipmask
	bvec3 ray_neg = lessThan(dir, vec3(0.0));
	vec3 flippedf = mix(pos, WORLD_SIZEf - pos, ray_neg);
	
	uvec3 flipmask = mix(uvec3(0u), uvec3(FLIPMASK), ray_neg);
	
	// precompute part of plane projection equation
	// prefer  'pos * inv_dir + bias'  over  'inv_dir * (pos - ray_pos)'
	// due to mad instruction
	//vec3 inv_dir = mix(1.0 / abs(dir), vec3(INF), equal(dir, vec3(0.0)));
	vec3 inv_dir = 1.0 / abs(dir);
	vec3 bias = inv_dir * -flippedf;
	
	float dist = 0.0;
	#if 1 // allow ray to start outside ray for nice debugging views
	{
		// calculate entry and exit coords into whole world cube
		vec3 t0v = inv_dir * -flippedf;
		vec3 t1v = inv_dir * (vec3(WORLD_SIZEf) - flippedf);
		float t0 = max(max(t0v.x, t0v.y), t0v.z);
		float t1 = min(min(t1v.x, t1v.y), t1v.z);
		
		// only if ray not inside cube
		t0 = max(t0, 0.0);
		t1 = max(t1, 0.0);
		
		// ray misses world texture
		if (t1 <= t0)
			return false;
		
		// adjust ray to start where it hits cube initally
		dist = t0;
		flippedf += abs(dir) * dist;
		flippedf = max(flippedf, vec3(0.0));
	}
	#else
	// cull any rays starting outside of cube
	if ( any(lessThanEqual(flippedf, vec3(0.0))) ||
		 any(greaterThanEqual(flippedf, vec3(WORLD_SIZEf))))
		return false;
	#endif
	
	// start at some level of octree
	// -best to start at 0 if camera on surface
	// -best at higher levels if camera were in a large empty region
	uint mip = 0;
	//uint mip = uint(OCTREE_MIPS-1);
	
	// round down to start cell of octree
	uvec3 coord = uvec3(floor(flippedf));
	coord &= ROUNDMASK << mip;
	
	uint voxel;
	
	for (;;) {
		VISUALIZE_COST_COUNT
	
		uvec3 flipped = (coord ^ flipmask) >> mip;
		
		// read octree cell
		voxel = texelFetch(octree, ivec3(flipped), int(mip)).r;
		
		if (voxel != medium_bid) {
			// non-air octree cell
			if (mip == 0u)
				break; // found solid leaf voxel
			
			// decend octree
			mip--;
			uvec3 next_coord = coord + (1u << mip);
			
			// upate coord by determining which child octant is entered first
			// by comparing ray hit against middle plane hits
			vec3 tmidv = inv_dir * vec3(next_coord) + bias;
			
			coord = mix(coord, next_coord, lessThan(tmidv, vec3(dist)));
			
		} else {
			// air octree cell, continue stepping
			uvec3 next_coord = coord + (1u << mip);
			
			// calculate exit distances of next octree cell
			vec3 t0v = inv_dir * vec3(next_coord) + bias;
			dist = min(min(t0v.x, t0v.y), t0v.z);
			
			// step on axis where exit distance is lowest
			uint stepcoord;
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
			
			
			#if 0
			// step up to highest changed octree parent cell
			mip = findLSB(stepcoord);
			#else
			// step up one level
			// (does not work if lower mips cannot be safely read without reading higher levels)
			// also breaks  mip >= uint(OCTREE_MIPS-1)  as world exit condition
			
			//mip += min(findLSB(stepcoord >> mip) - mip, 1u);
			mip += bitfieldExtract(stepcoord, int(mip), 1) ^ 1; // extract lowest bit of coord 
			#endif
			
			// round down coord to lower corner of (potential) parent cell
			coord &= ROUNDMASK << mip;
			
			//// exit when either stepped out of world or max ray dist reached
			//if (mip >= uint(OCTREE_MIPS-1) || dist >= max_dist)
			if (stepcoord >= WORLD_SIZE || dist >= max_dist) {
				#if DEBUGDRAW
				if (_dbgdraw_rays) dbgdraw_vector(pos - WORLD_SIZEf/2.0, dir * dist, _dbg_ray_cols[type]);
				#endif
				return false;
			}
		}
	}
	
	#if DEBUGDRAW
	if (_dbgdraw_rays) dbgdraw_vector(pos - WORLD_SIZEf/2.0, dir * dist, _dbg_ray_cols[type]);
	#endif
	
	if (type < RAYT_SUN) {
		coord ^= flipmask; // flip back to real coords
		
		// arrived at solid leaf voxel, read block id from seperate data structure
		//hit.bid = read_bid(ivec3(coord));
		hit.bid = voxel;
		hit.medium = medium_bid;
		
		// calcualte surface hit info
		hit.dist = dist;
		hit.pos = pos + dir * dist;
		
		vec2 uv;
		int face;
		{ // calc hit face, uv and normal
			//vec3 hit_fract = fract(hit.pos);
			vec3 hit_fract = hit.pos;
			vec3 hit_center = vec3(coord) + 0.5;
			
			vec3 offs = (hit.pos - hit_center);
			vec3 abs_offs = abs(offs);
			
			vec3 normal = vec3(0.0);
			
			if (abs_offs.x >= abs_offs.y && abs_offs.x >= abs_offs.z) {
				normal.x = sign(offs.x);
				face = offs.x < 0.0 ? 0 : 1;
				uv = hit_fract.yz;
				if (offs.x < 0.0) uv.x = 1.0 - uv.x;
			} else if (abs_offs.y >= abs_offs.z) {
				normal.y = sign(offs.y);
				face = offs.y < 0.0 ? 2 : 3;
				uv = hit_fract.xz;
				if (offs.y >= 0.0) uv.x = 1.0 - uv.x;
			} else {
				normal.z = sign(offs.z);
				face = offs.z < 0.0 ? 4 : 5;
				uv = hit_fract.xy;
				if (offs.z < 0.0) uv.y = 1.0 - uv.y;
			}
			
			//{
			//	vec3 sphere_pos = hit_center;
			//	float sphere_r = 0.5;
			//	
			//	vec3 pos_rel = sphere_pos - pos;
			//	
			//	float t = dot(pos_rel, dir);
			//	vec3 closest = pos + dir * t;
			//	
			//	
			//	
			//	float hit_r = length(closest - sphere_pos);
			//	if (hit_r <= sphere_r) {
			//		float depth = sqrt(sphere_r*sphere_r - hit_r*hit_r);
			//		
			//		t -= depth;
			//		
			//		hit.pos = pos + dir * t;
			//		normal = normalize(hit.pos - sphere_pos);
			//	}
			//	
			//}
			
			hit.TBN = calc_TBN(normal, generate_tangent(normal));
		}
		
		uint tex_bid = hit.bid == B_AIR ? medium_bid : hit.bid;
		float texid = float(block_tiles[tex_bid].sides[face]);
		
		float lod2 = log2(dist)*0.90 - 2.0;
		
		hit.col = textureLod(tile_textures, vec3(uv, texid), log2(dist)*0.20 - 0.7).rgb;
		
		if (tex_bid == B_TALLGRASS && face >= 4)
			hit.col = vec3(0.0);
	
		hit.occl_spec.x = 1.0;
		hit.occl_spec.y = 1.0;
		
		hit.emiss = get_emmisive(hit.bid);
	}
	return true;
}

uniform bool  visualize_light = false;

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

vec3 random_in_sphere () {
	vec3 rnd = rand3();
    float theta = rnd.x * 2.0 * PI;
    float phi = acos(2.0 * rnd.y - 1.0);
    float r = pow(rnd.z, 0.33333333);
    float sp = sin(phi);
    float cp = cos(phi);
    return r * vec3(sp * sin(theta), sp * cos(theta), cp);
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
#endif
