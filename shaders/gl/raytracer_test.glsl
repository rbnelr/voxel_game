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
#define SUBCHUNK_COUNT		(CHUNK_SIZE / SUBCHUNK_SIZE) // size of chunk in subchunks per axis
#define SUBCHUNK_SHIFT		2
#define SUBCHUNK_MASK		3
#else
#define SUBCHUNK_SIZE		8 // size of subchunk in blocks per axis
#define SUBCHUNK_COUNT		(CHUNK_SIZE / SUBCHUNK_SIZE) // size of chunk in subchunks per axis
#define SUBCHUNK_SHIFT		3
#define SUBCHUNK_MASK		7
#endif

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

struct Chunk {
	uint flags;
	//ivec3 pos;
	int posx; int posy; int posz;

	//uint16_t neighbours[6];
	uint _neighbours[3];

	//uint16_t opaque_mesh_slices;
	//uint16_t transp_mesh_slices;
	uint _transp_mesh_slices;

	uint opaque_mesh_vertex_count;
	uint transp_mesh_vertex_count;
};
struct ChunkVoxels {
	// data for all subchunks
	// sparse subchunk:  block_id of all subchunk voxels
	// dense  subchunk:  id of subchunk
	uint sparse_data[SUBCHUNK_COUNT*SUBCHUNK_COUNT*SUBCHUNK_COUNT];

	// packed bits for all subchunks, where  0: dense subchunk  1: sparse subchunk
	uint sparse_bits[SUBCHUNK_COUNT*SUBCHUNK_COUNT*SUBCHUNK_COUNT / 32];
};
struct SubchunkVoxels {
	uint voxels[SUBCHUNK_SIZE][SUBCHUNK_SIZE][SUBCHUNK_SIZE/2];
};

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

layout(std430, binding = 3) readonly buffer Chunks {
	Chunk chunks[];
};
layout(std430, binding = 4) readonly buffer _ChunkVoxels {
	ChunkVoxels chunk_voxels[];
};
layout(std430, binding = 5) readonly buffer Subchunks {
	SubchunkVoxels subchunks[];
};

layout(rgba16f, binding = 6) uniform image2D img;

//
ivec3 get_pos (uint cid) {
	return ivec3(chunks[cid].posx, chunks[cid].posy, chunks[cid].posz);
}
uint get_neighbour (uint cid, int i) {
	return (chunks[cid]._neighbours[i >> 1] >> ((i & 1) * 16)) & 0xffffu;
}

int get_subchunk_idx (ivec3 pos) {
	pos >>= SUBCHUNK_SHIFT;
	return pos.z * SUBCHUNK_COUNT*SUBCHUNK_COUNT + pos.y * SUBCHUNK_COUNT + pos.x;
}
bool is_subchunk_sparse (uint cid, int subc_i) {
	uint test = chunk_voxels[cid].sparse_bits[subc_i >> 5] & (1u << (subc_i & 31));
	return test != 0u;
}

uint get_voxel (uint subc_id, ivec3 pos) {
	uint val = subchunks[subc_id].voxels[pos.z][pos.y][pos.x >> 1];
	return (val >> ((pos.x & 1) * 16)) & 0xffffu;
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

uniform bool  bouncelight_enable = true;
uniform float bouncelight_dist = 30.0;

uniform int   rays = 1;

uniform bool  visualize_light = false;

const vec3 sun_dir = normalize(vec3(-1,2,3));
const float sun_dir_rand = 0.05;

//
struct Hit {
	vec3	pos;
	vec3	normal;
	uint	chunkid;
	float	dist;
	vec3	col;
	vec3	emiss;
};

int iterations = 0;

// get pixel ray in world space based on pixel coord and matricies
void get_ray (vec2 px_pos, out uint cam_chunk, out vec3 ray_pos, out vec3 ray_dir) {
	
	vec2 px_jitter = rand2();
	vec2 ndc = (px_pos + px_jitter) / view.viewport_size * 2.0 - 1.0;
	//vec2 ndc = (px_pos + 0.5) / view.viewport_size * 2.0 - 1.0;
	
	vec4 clip = vec4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;

	// TODO: can't get optimization to one single clip_to_world mat mul to work, why?
	vec3 cam = (view.clip_to_cam * clip).xyz;

	ray_dir = (view.cam_to_world * vec4(cam, 0)).xyz;
	ray_dir = normalize(ray_dir);
	
	// ray starts on the near plane
	//ray_pos = (view.cam_to_world * vec4(cam, 1)).xyz;
	
	// all rays starts exactly on the camera
	ray_pos = (view.cam_to_world * vec4(0,0,0,1)).xyz;
	
	// try to fix edge case where camera is on exactly a chunk boundary
	// this is not ideal at all, and can cause artefacts
	if (fract(ray_pos.x / float(CHUNK_SIZE)) == 0.0) ray_pos.x += 0.0001;
	if (fract(ray_pos.y / float(CHUNK_SIZE)) == 0.0) ray_pos.y += 0.0001;
	if (fract(ray_pos.z / float(CHUNK_SIZE)) == 0.0) ray_pos.z += 0.0001;
	
	cam_chunk = camera_chunk;
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

bool hit_voxel (uint bid, inout uint prev_bid, vec3 ray_pos, vec3 ray_dir, float dist, int axis, uint chunkid, out Hit hit) {
	if (bid == B_LEAVES || bid == B_TALLGRASS) {
		if (prev_bid == 0)
			return false;
	} else {
		if (prev_bid == 0 || prev_bid == bid)
			return false;
	}
	
	if (bid == B_AIR)
		bid = prev_bid;
	prev_bid = bid;
	
	int entry_face = get_step_face(axis, ray_dir);
	
	hit.pos = ray_pos + ray_dir * dist;

	vec2 uv = calc_uv(fract(hit.pos), entry_face);

	float texid = float(block_tiles[bid].sides[entry_face]);
	
	//vec4 col = textureLod(tile_textures, vec3(uv, texid), log2(dist) - 5.8);
	vec4 col = texture(tile_textures, vec3(uv, texid), 0.0);
	
	if (bid == B_TALLGRASS && axis == 2)
		col = vec4(0.0);
	
	if (col.a <= 0.001)
		return false;
	
	hit.normal = vec3(0.0);
	hit.normal[axis] = -sign(ray_dir[axis]);
	
	hit.chunkid = chunkid;
	hit.dist = dist;
	hit.col = col.rgb;
	hit.emiss = col.rgb * get_emmisive(bid);
	return true;
}

bool trace_ray (vec3 ray_pos, vec3 ray_dir, uint chunkid, float max_dist, out Hit hit) {

	#define SUBCHUNK_SIZEf float(SUBCHUNK_SIZE)

	ivec3 sign;
	sign.x = ray_dir.x >= 0.0 ? 1 : -1;
	sign.y = ray_dir.y >= 0.0 ? 1 : -1;
	sign.z = ray_dir.z >= 0.0 ? 1 : -1;

	vec3 rdir;
	rdir.x = ray_dir.x != 0.0 ? 1.0 / abs(ray_dir.x) : INF;
	rdir.y = ray_dir.y != 0.0 ? 1.0 / abs(ray_dir.y) : INF;
	rdir.z = ray_dir.z != 0.0 ? 1.0 / abs(ray_dir.z) : INF;

	vec3 scoordf = floor(ray_pos / SUBCHUNK_SIZEf) * SUBCHUNK_SIZEf;
	ivec3 scoord = ivec3(scoordf);

	vec3 snext;
	{
		vec3 rel = ray_pos - scoordf;
		snext.x = rdir.x * (ray_dir.x >= 0.0 ? SUBCHUNK_SIZEf - rel.x : rel.x);
		snext.y = rdir.y * (ray_dir.y >= 0.0 ? SUBCHUNK_SIZEf - rel.y : rel.y);
		snext.z = rdir.z * (ray_dir.z >= 0.0 ? SUBCHUNK_SIZEf - rel.z : rel.z);
	}

	int stepmask;
	float dist = 0; 
	int axis = 0;

	uint prev_bid = 0;
	uint prev_chunkid = chunkid;

	while (chunkid != 0xffffu) {
		scoord &= CHUNK_SIZE_MASK;
		
		do {
			int subci = get_subchunk_idx(scoord);
			
			uint subchunk_data = chunk_voxels[chunkid].sparse_data[subci];
			if (is_subchunk_sparse(chunkid, subci)) {
				uint bid = subchunk_data;
				
				if (hit_voxel(bid, prev_bid, ray_pos, ray_dir, dist, axis, prev_chunkid, hit))
					return true;

				prev_bid = bid;
				prev_chunkid = chunkid;
				
				if (++iterations >= max_iterations || dist >= max_dist)
					return false; // max dist reached
			#if VISUALIZE_COST && VISUALIZE_WARP_COST
				if (subgroupElect())
					atomicAdd(warp_iter[gl_SubgroupID], 1u);
			#endif

			} else {
				
				#if 1
				vec3 proj = ray_pos + ray_dir * dist;
				if (dist > 0.0)
					proj[axis] += ray_dir[axis] >= 0 ? 0.5 : -0.5;
				vec3 coordf = floor(proj);
				#else
				vec3 coordf = floor(ray_pos + ray_dir * (dist + 0.001));
				#endif
				
				ivec3 coord = ivec3(coordf);
				
				vec3 next;
				{
					vec3 rel = ray_pos - coordf;
					next.x = rdir.x * (ray_dir.x >= 0.0f ? 1.0f - rel.x : rel.x);
					next.y = rdir.y * (ray_dir.y >= 0.0f ? 1.0f - rel.y : rel.y);
					next.z = rdir.z * (ray_dir.z >= 0.0f ? 1.0f - rel.z : rel.z);
				}
				
				coord &= SUBCHUNK_MASK;
				
				do {
					uint bid = get_voxel(subchunk_data, coord);
					
					if (hit_voxel(bid, prev_bid, ray_pos, ray_dir, dist, axis, prev_chunkid, hit))
						return true;

					prev_bid = bid;
					prev_chunkid = chunkid;
					
					//
					dist = min(min(next.x, next.y), next.z);
					bvec3 mask = equal(vec3(dist), next);
					
					axis = get_axis(mask);
					
					coord += mix(ivec3(0.0), sign, mask);
					next  += mix( vec3(0.0), rdir, mask);
					
					if (++iterations >= max_iterations || dist >= max_dist)
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

			stepmask = scoord.x | scoord.y | scoord.z;
		} while ((stepmask & ~CHUNK_SIZE_MASK) == 0);
		
		// stepped out of chunk
		chunkid = get_neighbour(chunkid, get_step_face(axis, ray_dir) ^ 1);
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

vec3 collect_sunlight (vec3 pos, vec3 normal, uint chunkid) {
	if (sunlight_enable) {
		vec3 dir = sun_dir + rand3()*sun_dir_rand;
		float cos = dot(dir, normal);
		
		if (cos > 0.0) {
			Hit hit;
			if (!trace_ray(pos, dir, chunkid, sunlight_dist, hit))
				return sunlight_col * cos;
		}
	}
	return vec3(0.0);
}

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
	
	vec3 col = vec3(0.0);
	for (int i=0; i<rays; ++i) {
		// primary ray
		vec3 ray_pos, ray_dir;
		uint cam_chunk;
		get_ray(pos, cam_chunk, ray_pos, ray_dir);
		
		Hit hit;
		if (trace_ray(ray_pos, ray_dir, cam_chunk, INF, hit)) {
			hit.pos += hit.normal * 0.001;
			
			vec3 light = ambient_light;
			light += collect_sunlight(hit.pos, hit.normal, hit.chunkid);
			
			mat3 tangent_to_world = get_tangent_to_world(hit.normal);
			
			if (bouncelight_enable) {
				vec3 dir = tangent_to_world * hemisphere_sample(); // already cos weighted
				
				Hit hit2;
				if (trace_ray(hit.pos, dir, hit.chunkid, bouncelight_dist, hit2)) {
					vec3 light2 = collect_sunlight(hit2.pos, hit2.normal, hit2.chunkid);
					
					light += hit2.emiss;
					light += hit2.col * light2;
				}
			}
			
			if (visualize_light)
				hit.col = vec3(1.0);
			
			col += hit.emiss;
			col += hit.col * light;
		}
	}
	col *= 1.0 / float(rays);

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
