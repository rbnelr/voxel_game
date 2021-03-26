#version 460 core
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
	if (      bid == B_MAGMA   ) return  8.0;
	else if ( bid == B_CRYSTAL ) return	22.0;
	else if ( bid == B_URANIUM ) return  4.0;
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
uniform sampler2D prev_framebuffer;

uniform mat4 prev_world2clip;

uniform float taa_alpha = 0.05;

#define SUBC_SPARSE_BIT 0x80000000u

uniform usampler3D	voxels[2];

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

const vec3 sun_pos = vec3(-28, 67, 102);
const float sun_pos_size = 4.0;

const vec3 sun_dir = normalize(vec3(-1,2,3));
const float sun_dir_rand = 0.05;

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

// get pixel ray in world space based on pixel coord and matricies
void get_ray (vec2 px_pos, out vec3 ray_pos, out vec3 ray_dir) {
	
	//vec2 px_center = rand2();
	vec2 px_center = vec2(0.5);
	vec2 ndc = (px_pos + px_center) / view.viewport_size * 2.0 - 1.0;
	//vec2 ndc = (px_pos + 0.5) / view.viewport_size * 2.0 - 1.0;
	
	vec4 clip = vec4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;

	// TODO: can't get optimization to one single clip_to_world mat mul to work, why?
	vec3 cam = (view.clip_to_cam * clip).xyz;

	ray_dir = (view.cam_to_world * vec4(cam, 0)).xyz;
	ray_dir = normalize(ray_dir);
	
	// ray starts on the near plane
	ray_pos = (view.cam_to_world * vec4(cam, 1)).xyz;
}

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

bool hit_voxel (uint bid, uint prev_bid, int axis, float dist,
		vec3 ray_pos, vec3 ray_dir, ivec3 flipmask, out Hit hit) {
	if (prev_bid == 0 || (bid == prev_bid && (bid != B_LEAVES && bid != B_TALLGRASS)))
		return false;
	
	vec3 hit_pos = (ray_pos + ray_dir * dist) * mix(vec3(-1), vec3(1), equal(flipmask, ivec3(0.0)));
	
	int entry_face = get_step_face(axis, flipmask);
	vec2 uv = calc_uv(fract(hit_pos), axis, entry_face);
	
	uint tex_bid = bid == B_AIR ? prev_bid : bid;
	float texid = float(block_tiles[tex_bid].sides[entry_face]);
	
	//vec4 col = textureLod(tile_textures, vec3(uv, texid), log2(dist) - 5.8);
	vec4 col = texture(tile_textures, vec3(uv, texid), 0.0);
	
	if (tex_bid == B_TALLGRASS && axis == 2)
		col = vec4(0.0);
	
	if (col.a <= 0.001)
		return false;
	
	hit.pos = hit_pos;
	hit.normal = mix(vec3(0.0), mix(ivec3(+1), ivec3(-1), equal(flipmask, ivec3(0))), equal(ivec3(axis), ivec3(0,1,2)));
	
	hit.bid = bid;
	hit.prev_bid = prev_bid;
	
	hit.dist = dist;
	hit.col = col.rgb;
	hit.emiss = col.rgb * get_emmisive(hit.bid);
	return true;
}

#if 0
bool trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit) {
	
	ivec3 flipmask = mix(ivec3(0), ivec3(-1), lessThan(ray_dir, vec3(0.0)));
	ray_pos       *= mix(vec3(1), vec3(-1), lessThan(ray_dir, vec3(0.0)));
	
	ray_dir = abs(ray_dir);
	
	vec3 rdir = mix(1.0 / ray_dir, vec3(INF), equal(ray_dir, vec3(0.0)));
	ivec3 coord = ivec3(floor(ray_pos / float(SUBCHUNK_SIZE))) * SUBCHUNK_SIZE;
	
	float dist = 0; 
	int axis;
	
	uint prev_bid = 0;
	uint bid = 0;
	
	for (;;) {
		uint subchunk;
		{
			ivec3 _coord = (coord ^ flipmask) + 16 * CHUNK_SIZE;
		
			if (!all(lessThan(uvec3(_coord), uvec3(32 * CHUNK_SIZE))))
				return false;
			
			subchunk = texelFetch(voxels[0], _coord >> SUBCHUNK_SHIFT, 0).r;
		}
		
		if ((subchunk & SUBC_SPARSE_BIT) != 0) {
			bid = subchunk & ~SUBC_SPARSE_BIT;
			
			if (bid == 0)
				return false; // unloaded chunk
			
			if (hit_voxel(bid, prev_bid, axis, dist, ray_pos, ray_dir, flipmask, hit))
				return true;
			prev_bid = bid;
			
		#if VISUALIZE_COST && VISUALIZE_WARP_COST
			if (subgroupElect())
				atomicAdd(warp_iter[gl_SubgroupID], 1u);
		#endif
			if (++iterations >= max_iterations || dist >= max_dist)
				return false; // max dist reached
		
			vec3 next = rdir * (vec3(coord + SUBCHUNK_SIZE) - ray_pos);
			
			dist = min(min(next.x, next.y), next.z);
			
			if (next.x == dist) {
				axis = 0;
				coord.x += SUBCHUNK_SIZE;
			} else if (next.y == dist) {
				axis = 1;
				coord.y += SUBCHUNK_SIZE;
			} else {
				axis = 2;
				coord.z += SUBCHUNK_SIZE;
			}
			
		} else {
			
			ivec3 subc_offs = subchunk_id_to_texcoords(subchunk);
			
			{
				vec3 proj = ray_pos + ray_dir * dist;
				coord = clamp(ivec3(floor(proj)), coord, coord + ivec3(SUBCHUNK_SIZE-1));
			}
			
			for (;;) {
				bid = texelFetch(voxels[1], subc_offs + ((coord ^ flipmask) & SUBCHUNK_MASK), 0).r;
				
				if (hit_voxel(bid, prev_bid, axis, dist, ray_pos, ray_dir, flipmask, hit))
					return true;
				prev_bid = bid;
				
			#if VISUALIZE_COST && VISUALIZE_WARP_COST
				if (subgroupElect())
					atomicAdd(warp_iter[gl_SubgroupID], 1u);
			#endif
				if (++iterations >= max_iterations || dist >= max_dist)
					return false; // max dist reached
				
				vec3 next = rdir * (vec3(coord + 1) - ray_pos);
				dist = min(min(next.x, next.y), next.z);
				
				int new_coord;
				if (next.x == dist) {
					axis = 0;
					coord.x += 1;
					new_coord = coord.x;
				} else if (next.y == dist) {
					axis = 1;
					coord.y += 1;
					new_coord = coord.y;
				} else {
					axis = 2;
					coord.z += 1;
					new_coord = coord.z;
				}
				
				if ((new_coord & SUBCHUNK_MASK) == 0)
					break;
			}
			
			// stepped out of subchunk
			
			coord &= ~SUBCHUNK_MASK;
		}
	}
}
#else
bool trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit) {
	
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
		if ((new_coord & SUBCHUNK_MASK) == 0) {
			coord &= ~SUBCHUNK_MASK;
			
			ivec3 scoord = (coord ^ flipmask) + 16 * CHUNK_SIZE;
			
			if (!all(lessThan(uvec3(scoord), uvec3(32 * CHUNK_SIZE))))
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
		
	#if VISUALIZE_COST && VISUALIZE_WARP_COST
		if (subgroupElect())
			atomicAdd(warp_iter[gl_SubgroupID], 1u);
	#endif
		if (++iterations >= max_iterations || dist >= max_dist)
			return false; // max dist reached
		
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
#endif

#if !ONLY_PRIMARY_RAYS
bool trace_ray_refl_refr (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit) {
	for (int j=0; j<2; ++j) {
		if (!trace_ray(ray_pos, ray_dir, max_dist, hit))
			break;
		
		bool water = hit.bid == B_WATER || hit.prev_bid == B_WATER;
		bool air = hit.bid == B_AIR || hit.prev_bid == B_AIR;
		bool water_surface = water && air;
		
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
		#if 0
		// directional sun
		vec3 dir = sun_dir + rand3()*sun_dir_rand;
		float cos = dot(dir, normal);
		
		if (cos > 0.0) {
			Hit hit2;
			if (!trace_ray(pos, dir, sunlight_dist, hit2))
				return sunlight_col * cos;
		}
		#else
		// point sun
		vec3 offs = (sun_pos + (rand3()-0.5) * sun_pos_size) - pos;
		float dist = length(offs);
		vec3 dir = normalize(offs);
		
		float cos = dot(dir, normal);
		float atten = 16000.0 / (dist*dist);
		
		if (cos > 0.0) {
			Hit hit2;
			float max_dist = dist - sun_pos_size*0.5;
			if (!trace_ray(pos, dir, max_dist, hit2))
				return sunlight_col * cos * atten;
		}
		#endif
	}
	return vec3(0.0);
}
#endif

uniform ivec2 dispatch_size;

void main () {
#if VISUALIZE_COST && VISUALIZE_WARP_COST
	if (subgroupElect()) {
		warp_iter[gl_SubgroupID] = 0u;
	}

	barrier();
#endif
	
	uvec2 pxpos = gl_GlobalInvocationID.xy;
	
	// maybe try not to do rays that we do not see (happens due to local group size)
	if (pxpos.x >= view.viewport_size.x || pxpos.y >= view.viewport_size.y)
		return;
	
	srand((gl_GlobalInvocationID.y << 16) + gl_GlobalInvocationID.x); // convert 2d pixel index to 1d value
	
	#if ONLY_PRIMARY_RAYS
	vec3 ray_pos, ray_dir;
	get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	Hit hit;
	bool did_hit = trace_ray(ray_pos, ray_dir, INF, hit);
	vec3 col = did_hit ? hit.col : vec3(0.0);
	
	#else
	// primary ray
	vec3 ray_pos, ray_dir;
	get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	vec3 col = vec3(0.0);
	
	Hit hit;
	bool did_hit = trace_ray_refl_refr(ray_pos, ray_dir, INF, hit);
	if (did_hit) {
		vec3 pos = hit.pos + hit.normal * 0.001;
		
		vec3 light = ambient_light;
		light += collect_sunlight(pos, hit.normal);
		
		if (bounces_enable) {
			
			float max_dist = bounces_max_dist;
			
			vec3 cur_normal = hit.normal;
			vec3 contrib = vec3(1.0);
			
			for (int j=0; j<bounces_max_count; ++j) {
				pos += cur_normal * 0.001;
				vec3 dir = get_tangent_to_world(cur_normal) * hemisphere_sample(); // already cos weighted
				
				Hit hit2;
				if (!trace_ray_refl_refr(pos, dir, max_dist, hit2))
					break;
				
				vec3 light2 = collect_sunlight(pos, cur_normal);
				
				light += (hit2.emiss + hit2.col * light2) * contrib;
				
				pos = hit2.pos + hit2.normal * 0.001;
				max_dist -= hit2.dist;
				
				cur_normal = hit2.normal;
				contrib *= hit2.col;
			}
		}
		
		if (visualize_light)
			hit.col = vec3(1.0);
		
		col += hit.emiss + hit.col * light;
	}
	#endif
	
	uint hit_id = 0;
	if (did_hit) {
		vec4 prev_clip = prev_world2clip * vec4(hit.pos, 1.0);
		prev_clip.xyz /= prev_clip.w;
		
		vec2 uv = prev_clip.xy * 0.5 + 0.5;
		if (all(greaterThan(uv, vec2(0.0))) && all(lessThan(uv, vec2(1.0)))) {
			vec4 prev_val = texture(prev_framebuffer, uv);
			
			vec3 prev_col = prev_val.rgb;
			uint prev_bid = packHalf2x16(vec2(prev_val.a, 0.0));
			
			if (prev_bid == hit.bid)
				col = mix(prev_col, col, vec3(taa_alpha));
		}
		
		hit_id = hit.bid;
	}

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
	
	imageStore(img, ivec2(pxpos), vec4(col, unpackHalf2x16(hit_id).x));
}
