#version 460 core
//#extension GL_ARB_gpu_shader5 : enable
//#extension GL_EXT_shader_16bit_storage : enable // not supported
#if VISUALIZE_COST && VISUALIZE_WARP_COST
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_ARB_shader_group_vote : enable
#endif

layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y) in;

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
	
	vec2 uv = rand2();
	
	float r = sqrt(uv.y);
	float theta = 2*PI * uv.x;
	
	float x = r * cos(theta);
	float y = r * sin(theta);
	
	vec3 dir = vec3(x,y, sqrt(max(0.0, 1.0 - uv.y)));
	return dir;
}

// TODO: could optimize this to be precalculated for all normals, since we currently only have 6
mat3 get_tangent_to_world (vec3 normal) {
	vec3 tangent = abs(normal.x) >= 0.9 ? vec3(0,1,0) : vec3(1,0,0);
	vec3 bitangent = cross(normal, tangent);
	tangent = cross(bitangent, normal);
	
	return mat3(tangent, bitangent, normal);
}

#if 1
#define CHUNK_SIZE			64 // size of chunk in blocks per axis
#define CHUNK_SIZE_SHIFT	6 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		63
#else
#define CHUNK_SIZE			32 // size of chunk in blocks per axis
#define CHUNK_SIZE_SHIFT	5 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		31
#endif

#if 0
#define SUBCHUNK_SIZE		4 // size of subchunk in blocks per axis
#define SUBCHUNK_SHIFT		2
#define SUBCHUNK_MASK		3
#else
#define SUBCHUNK_SIZE		8 // size of subchunk in blocks per axis
#define SUBCHUNK_SHIFT		3
#define SUBCHUNK_MASK		7
#endif

#define SUBCHUNK_COUNT		(CHUNK_SIZE / SUBCHUNK_SIZE) // size of chunk in subchunks per axis

#define B_AIR 1
#define B_WATER 3
#define B_MAGMA 12
#define B_CRYSTAL 15
#define B_URANIUM 16
#define B_LEAVES 18
#define B_TORCH 19
#define B_TALLGRASS 20

float get_emmisive (uint bid) {
	if (      bid == B_MAGMA   ) return 8.0;
	else if ( bid == B_CRYSTAL ) return	4.0;
	else if ( bid == B_URANIUM ) return 4.0;
	return 0.0;
}

// common_ubo       binding = 0
// block_meshes_ubo binding = 1
struct BlockTile {
	int sides[6];
	
	int anim_frames;
	int variants;
};
layout(std430, binding = 2) readonly buffer BlockTiles {
	BlockTile block_tiles[];
};

layout(rgba16f, binding = 3) uniform image2D img;

#define SUBC_SPARSE_BIT 0x80000000u

uniform usampler3D	subchunks_tex;
uniform usampler3D	voxels_tex;

#define TEX3D_SIZE			2048 // max width, height, depth for 3d textures

#define SUBCHUNK_TEX_COUNT	(TEX3D_SIZE / SUBCHUNK_SIZE) // max num of subchunks in one axis for tex
#define SUBCHUNK_TEX_SHIFT	8
#define SUBCHUNK_TEX_MASK	((SUBCHUNK_TEX_COUNT-1) << SUBCHUNK_SHIFT)

// subchunk id to 3d tex offset (including subchunk_size multiplication)
ivec3 subchunk_id_to_texcoords (uint id) {
	ivec3 coord;
	coord.x = int((id << (SUBCHUNK_TEX_SHIFT*0 + SUBCHUNK_SHIFT)) & SUBCHUNK_TEX_MASK);
	coord.y = int((id >> (SUBCHUNK_TEX_SHIFT*1 - SUBCHUNK_SHIFT)) & SUBCHUNK_TEX_MASK);
	coord.z = int((id >> (SUBCHUNK_TEX_SHIFT*2 - SUBCHUNK_SHIFT)) & SUBCHUNK_TEX_MASK);
	return coord;
}

uniform sampler2DArray	tile_textures;

uniform uint camera_chunk;
uniform int max_iterations = 200;

#if VISUALIZE_COST
uniform sampler2D		heat_gradient;
	#if VISUALIZE_WARP_COST
		#define WARP_COUNT_IN_WG 2
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

const vec3 sun_pos = vec3(-28, 67, 102);
const float sun_pos_size = 4.0;

const vec3 sun_dir = normalize(vec3(-1,2,3));
const float sun_dir_rand = 0.05;

const float water_F0 = 0.2;
const float water_IOR = 1.333;
const float air_IOR = 1.0;

//
struct Ray {
	vec3	pos;
	vec3	dir;
	float	max_dist;
};

struct Hit {
	//vec3	pos;
	//vec3	normal;
	//uint	chunkid;
	//float	dist;
	//uint	bid;
	//uint	prev_bid;
	vec3	col;
	//vec3	emiss;
};

int iterations = 0;

// get pixel ray in world space based on pixel coord and matricies
void get_ray (vec2 px_pos, out vec3 ray_pos, out vec3 ray_dir) {
	
	vec2 px_jitter = rand2();
	vec2 ndc = (px_pos + px_jitter) / view.viewport_size * 2.0 - 1.0;
	//vec2 ndc = (px_pos + 0.5) / view.viewport_size * 2.0 - 1.0;
	
	vec4 clip = vec4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;

	// TODO: can't get optimization to one single clip_to_world mat mul to work, why?
	vec3 cam = (view.clip_to_cam * clip).xyz;

	ray_dir = (view.cam_to_world * vec4(cam, 0)).xyz;
	ray_dir = normalize(ray_dir);
	
	// ray starts on the near plane
	ray_pos = (view.cam_to_world * vec4(cam, 1)).xyz;
}

int get_axis (bvec3 axis_mask) {
	if (     axis_mask.x) return 0;
	else if (axis_mask.y) return 1;
	else                  return 2;
}
int get_step_face (int axis, vec3 ray_dir) {
	return axis*2 +(ray_dir[axis] >= 0.0 ? 0 : 1);
}
vec2 calc_uv (vec3 pos_fract, int entry_face) {
	vec2 uv;
	if (entry_face / 2 == 0) {
		uv = pos_fract.yz;
	} else if (entry_face / 2 == 1) {
		uv = pos_fract.xz;
	} else {
		uv = pos_fract.xy;
	}

	if (entry_face == 0 || entry_face == 3)  uv.x = 1.0 - uv.x;
	if (entry_face == 4)                     uv.y = 1.0 - uv.y;

	return uv;
}

bool hit_voxel (uint bid, inout uint prev_bid, vec3 ray_pos, vec3 ray_dir, float dist, int axis, out Hit hit) {
	if (bid == B_LEAVES || bid == B_TALLGRASS) {
		if (prev_bid == 0)
			return false;
	} else {
		if (prev_bid == 0 || prev_bid == bid)
			return false;
	}
	
	int entry_face = get_step_face(axis, ray_dir);
	
	vec3 hit_pos = ray_pos + ray_dir * dist;

	vec2 uv = calc_uv(fract(hit_pos), entry_face);
	
	uint tex_bid = bid == B_AIR ? prev_bid : bid;
	float texid = float(block_tiles[tex_bid].sides[entry_face]);
	
	//vec4 col = textureLod(tile_textures, vec3(uv, texid), log2(dist) - 5.8);
	vec4 col = texture(tile_textures, vec3(uv, texid), 0.0);
	
	//if (tex_bid == B_TALLGRASS && axis == 2)
	//	col = vec4(0.0);
	
	if (col.a <= 0.001)
		return false;
	
	//hit.pos = hit_pos;
	//hit.normal = vec3(0.0);
	//hit.normal[axis] = -sign(ray_dir[axis]);
	//
	//hit.bid = bid;
	//hit.prev_bid = prev_bid;
	//
	//hit.chunkid = chunkid;
	//hit.dist = dist;
	hit.col = col.rgb;
	//hit.emiss = col.rgb * get_emmisive(bid);
	return true;
}

#define SUBCHUNK_SIZEf float(SUBCHUNK_SIZE)

bool trace_ray (Ray ray, out Hit hit) {
	ivec3 sign;
	sign.x = ray.dir.x >= 0.0 ? 1 : -1;
	sign.y = ray.dir.y >= 0.0 ? 1 : -1;
	sign.z = ray.dir.z >= 0.0 ? 1 : -1;

	vec3 rdir;
	rdir.x = ray.dir.x != 0.0 ? 1.0 / abs(ray.dir.x) : INF;
	rdir.y = ray.dir.y != 0.0 ? 1.0 / abs(ray.dir.y) : INF;
	rdir.z = ray.dir.z != 0.0 ? 1.0 / abs(ray.dir.z) : INF;

	vec3 scoordf = floor(ray.pos / SUBCHUNK_SIZEf) * SUBCHUNK_SIZEf;
	ivec3 scoord = ivec3(scoordf);

	vec3 snext;
	{
		vec3 rel = ray.pos - scoordf;
		snext.x = rdir.x * (ray.dir.x >= 0.0 ? SUBCHUNK_SIZEf - rel.x : rel.x);
		snext.y = rdir.y * (ray.dir.y >= 0.0 ? SUBCHUNK_SIZEf - rel.y : rel.y);
		snext.z = rdir.z * (ray.dir.z >= 0.0 ? SUBCHUNK_SIZEf - rel.z : rel.z);
	}

	int stepmask;
	float dist = 0; 
	int axis = 0;

	uint prev_bid = 0;
	
	scoord += 16 * CHUNK_SIZE;
	
	if (!all(lessThan(uvec3(scoord), uvec3(32 * CHUNK_SIZE))))
		return false;
	
	for (;;) {
		//scoord &= CHUNK_SIZE_MASK;
		scoord &= (64 << 5) -1;
		
		uint subchunk = texelFetch(subchunks_tex, scoord >> SUBCHUNK_SHIFT, 0).r;
		
		if ((subchunk & SUBC_SPARSE_BIT) != 0) {
			uint bid = subchunk & ~SUBC_SPARSE_BIT;
			
			if (bid == 0)
				return false; // unloaded chunk
			
			if (hit_voxel(bid, prev_bid, ray.pos, ray.dir, dist, axis, hit))
				return true;

			prev_bid = bid;
			
			if (++iterations >= max_iterations || dist >= ray.max_dist)
				return false; // max dist reached
		#if VISUALIZE_COST && VISUALIZE_WARP_COST
			if (subgroupElect())
				atomicAdd(warp_iter[gl_SubgroupID], 1u);
		#endif

		} else {
			
			ivec3 subc_offs = subchunk_id_to_texcoords(subchunk);
			
			#if 1
			vec3 proj = ray.pos + ray.dir * dist;
			if (dist > 0.0)
				proj[axis] += ray.dir[axis] >= 0 ? 0.5 : -0.5;
			vec3 coordf = floor(proj);
			#else
			vec3 coordf = floor(ray.pos + ray.dir * (dist + 0.001));
			#endif
			
			ivec3 coord = ivec3(coordf);
			
			vec3 next;
			{
				vec3 rel = ray.pos - coordf;
				next.x = rdir.x * (ray.dir.x >= 0.0f ? 1.0f - rel.x : rel.x);
				next.y = rdir.y * (ray.dir.y >= 0.0f ? 1.0f - rel.y : rel.y);
				next.z = rdir.z * (ray.dir.z >= 0.0f ? 1.0f - rel.z : rel.z);
			}
			
			coord &= SUBCHUNK_MASK;
			
			do {
				uint bid = texelFetch(voxels_tex, subc_offs + coord, 0).r;
				
				if (hit_voxel(bid, prev_bid, ray.pos, ray.dir, dist, axis, hit))
					return true;

				prev_bid = bid;
				
				//
				dist = min(min(next.x, next.y), next.z);
				bvec3 mask = equal(vec3(dist), next);
				
				axis = get_axis(mask);
				
				coord += mix(ivec3(0.0), sign, mask);
				next  += mix( vec3(0.0), rdir, mask);
				
				if (++iterations >= max_iterations || dist >= ray.max_dist)
					return false; // max dist reached
			#if VISUALIZE_COST && VISUALIZE_WARP_COST
				if (subgroupElect())
					atomicAdd(warp_iter[gl_SubgroupID], 1u);
			#endif
				
				stepmask = coord.x | coord.y | coord.z;
			} while ((stepmask & ~SUBCHUNK_MASK) == 0);
			//} while (dist < sdist - 0.0001);
			
			// stepped out of subchunk
		}
		
		dist = min(min(snext.x, snext.y), snext.z);
		bvec3 mask = equal(vec3(dist), snext);
		
		axis = get_axis(mask);
		
		scoord += mix(ivec3(0.0), sign * SUBCHUNK_SIZE, mask);
		snext  += mix( vec3(0.0), rdir * SUBCHUNK_SIZEf, mask);
		
		if (!all(lessThan(uvec3(scoord), uvec3(32 * CHUNK_SIZE))))
			return false;
	}
	
	return false;
}

vec3 tonemap (vec3 c) {

	//c *= 2.0;
	//c = c / (c + 1.0);

	//c *= 0.2;
	//vec3 x = max(vec3(0.0), c -0.004);
	//c = (x*(6.2*x+.5))/(x*(6.2*x+1.7)+0.06);
	return c;
}

#if 0
vec3 collect_sunlight (Hit hit) {
	if (sunlight_enable) {
		Ray r;
		r.pos = hit.pos;
		r.chunkid = hit.chunkid;
		
		#if 0
		// directional sun
		r.dir = sun_dir + rand3()*sun_dir_rand;
		float cos = dot(r.dir, hit.normal);
		
		if (cos > 0.0) {
			Hit hit;
			r.max_dist = sunlight_dist;
			if (!trace_ray(r, hit))
				return sunlight_col * cos;
		}
		#else
		// point sun
		vec3 offs = (sun_pos + (rand3()-0.5) * sun_pos_size) - hit.pos;
		float dist = length(offs);
		r.dir = normalize(offs);
		
		float cos = dot(r.dir, hit.normal);
		float atten = 10000.0 / (dist*dist);
		
		if (cos > 0.0) {
			Hit hit;
			r.max_dist = dist - sun_pos_size*0.5;
			if (!trace_ray(r, hit))
				return sunlight_col * cos * atten;
		}
		#endif
	}
	return vec3(0.0);
}
#endif

void main () {
#if VISUALIZE_COST && VISUALIZE_WARP_COST
	if (subgroupElect()) {
		warp_iter[gl_SubgroupID] = 0u;
	}

	barrier();
#endif

	vec2 pos = vec2(gl_GlobalInvocationID.xy);

	// maybe try not to do rays that we do not see (happens due to local group size)
	if (pos.x >= view.viewport_size.x || pos.y >= view.viewport_size.y)
		return;

	srand((gl_GlobalInvocationID.y << 16) + gl_GlobalInvocationID.x); // convert 2d pixel index to 1d value
	
	#if 1
	Ray ray;
	get_ray(pos, ray.pos, ray.dir);
	ray.max_dist = INF;
	
	vec3 col = vec3(0.0);
	
	Hit hit;
	if (trace_ray(ray, hit))
		col = hit.col;
	
	#else
	vec3 col = vec3(0.0);
	for (int i=0; i<rays; ++i) {
		// primary ray
		Ray ray;
		get_ray(pos, ray.pos, ray.dir);
		ray.max_dist = INF;
		
		for (int j=0; j<2; ++j) {
			Hit hit;
			if (!trace_ray(ray, hit))
				break;
			
			hit.pos += hit.normal * 0.001;
			
			vec3 light = ambient_light;
			light += collect_sunlight(hit);
			
			if (bounces_enable) {
				
				Ray ray2;
				ray2.pos = hit.pos;
				ray2.max_dist = bounces_max_dist;
				
				vec3 cur_normal = hit.normal;
				vec3 contrib = vec3(1.0);
				
				for (int j=0; j<bounces_max_count; ++j) {
					ray2.dir = get_tangent_to_world(cur_normal) * hemisphere_sample(); // already cos weighted
					
					Hit hit2;
					if (!trace_ray(ray2, hit2))
						break;
					
					vec3 light2 = collect_sunlight(hit2);
					
					light += hit2.emiss;
					light += hit2.col * light2;
					
					ray2.pos = hit2.pos += hit2.normal * 0.001;
					ray2.max_dist -= hit2.dist;
					
					cur_normal = hit2.normal;
					contrib *= hit2.col;
				}
			}
			
			if (visualize_light)
				hit.col = vec3(1.0);
			
			bool water = hit.bid == B_WATER || hit.prev_bid == B_WATER;
			bool air = hit.bid == B_AIR || hit.prev_bid == B_AIR;
			bool surface = water && air;
			
			if (!surface) {
				
				col += hit.emiss;
				col += hit.col * light;
				
				break;
			} else {
				
				float reflect_fac = fresnel(-ray.dir, hit.normal, water_F0);
				
				float eta = hit.bid == B_WATER ? air_IOR / water_IOR : water_IOR / air_IOR;
				
				vec3 reflect_dir = reflect(ray.dir, hit.normal);
				vec3 refract_dir = refract(ray.dir, hit.normal, eta);
				
				if (dot(refract_dir, refract_dir) == 0.0) {
					// total internal reflection, ie. outside of snells window
					reflect_fac = 1.0;
				}
				
				if (rand() <= reflect_fac) {
					// reflect
					ray.pos = hit.pos;
					ray.dir = reflect_dir;
				} else {
					// refract
					ray.pos = hit.pos - 2.0 * hit.normal * 0.001;
					ray.dir = refract_dir;
				}
				ray.chunkid = hit.chunkid;
			}
		}
	}
	col *= 1.0 / float(rays);
	#endif

#if VISUALIZE_COST
	#if VISUALIZE_WARP_COST
		const uint warp_cost = warp_iter[gl_SubgroupID];
		const uint local_cost = iterations;
		
		float wasted_work = float(warp_cost - local_cost) / float(warp_cost);
		col = texture(heat_gradient, vec2(wasted_work, 0.5)).rgb;
	#else
		col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5)).rgb;
	#endif
#endif
	imageStore(img, ivec2(pos), vec4(tonemap(col), 1.0));
}
