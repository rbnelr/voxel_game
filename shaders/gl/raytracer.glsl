#version 460 core
#if VISUALIZE_COST && VISUALIZE_WARP_COST
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_ARB_shader_group_vote : enable
#endif

layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y) in;

#include "common.glsl"

#define PI	3.1415926535897932384626433832795
#define INF (1. / 0.)

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

#define SUBC_SPARSE_BIT 0x80000000u

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

layout(rgba16f, binding = 4) uniform image2D img;

uniform usampler3D	chunk_voxels;
uniform usampler3D	subchunk_voxels;

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

//
ivec3 get_pos (uint cid) {
	return ivec3(chunks[cid].posx, chunks[cid].posy, chunks[cid].posz);
}
uint get_neighbour (uint cid, int i) {
	return (chunks[cid]._neighbours[i >> 1] >> ((i & 1) * 16)) & 0xffffu;
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

//
struct Ray {
	vec3	pos;
	vec3	dir;
	uint	chunkid;
	float	max_dist;
};

int iterations = 0;

// get pixel ray in world space based on pixel coord and matricies
void get_ray (vec2 px_pos, out uint cam_chunk, out vec3 ray_pos, out vec3 ray_dir) {
	
	vec2 ndc = (px_pos + 0.5) / view.viewport_size * 2.0 - 1.0;
	
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

int get_step_face (int axis, vec3 ray_dir) {
	return axis*2 + (ray_dir[axis] >= 0.0 ? 0 : 1);
}
vec2 calc_uv (vec3 pos, int entry_face) {
	vec2 uv;
	if (entry_face / 2 == 0) {
		uv = pos.yz;
	} else if (entry_face / 2 == 1) {
		uv = pos.xz;
	} else {
		uv = pos.xy;
	}
	uv = fract(uv);

	if (entry_face == 0 || entry_face == 3)  uv.x = 1.0 - uv.x;
	if (entry_face == 4)                     uv.y = 1.0 - uv.y;

	return uv;
}

bool hit_voxel (uint bid, inout uint prev_bid, vec3 ray_pos, vec3 ray_dir, float dist, int axis, out vec3 hit_col) {
	if (prev_bid == 0 || (prev_bid == bid && !(bid == B_LEAVES || bid == B_TALLGRASS)))
		return false;
	
	if (bid == B_AIR)
		bid = prev_bid;
	
	vec3 hit_pos = ray_pos + ray_dir * dist;

	int entry_face = get_step_face(axis, ray_dir);
	vec2 uv = calc_uv(hit_pos, entry_face);
	
	float texid = float(block_tiles[bid].sides[entry_face]);
	
	vec4 col = texture(tile_textures, vec3(uv, texid), 0.0);
	
	if (col.a <= 0.001)
		return false;
	
	hit_col = col.rgb;
	return true;
}

#define SUBCHUNK_SIZEf float(SUBCHUNK_SIZE)

bool trace_ray (Ray ray, out vec3 hit_col) {
	if (ray.chunkid == 0xffffu)
		return false;
	
	vec3 rdir;
	rdir.x = ray.dir.x != 0.0 ? 1.0 / abs(ray.dir.x) : INF;
	rdir.y = ray.dir.y != 0.0 ? 1.0 / abs(ray.dir.y) : INF;
	rdir.z = ray.dir.z != 0.0 ? 1.0 / abs(ray.dir.z) : INF;
	
	ivec3 coord = ivec3(floor(ray.pos / SUBCHUNK_SIZEf)) * SUBCHUNK_SIZE;
	
	float dist = 0; 
	int axis;
	uint prev_bid = 0;
	uint subchunk;
	int stepmask = -1;
	int stepsize = SUBCHUNK_SIZE;
	
	for (;;) {
		if ((stepmask & ~SUBCHUNK_MASK) != 0) {
			coord &= ~SUBCHUNK_MASK;
			
			ivec3 chunk_offs = subchunk_id_to_texcoords(ray.chunkid);
			subchunk = texelFetch(chunk_voxels, chunk_offs + ((coord & CHUNK_SIZE_MASK) >> SUBCHUNK_SHIFT), 0).r;
		}
		
		uint bid;
		
		if ((subchunk & SUBC_SPARSE_BIT) != 0) {
			stepsize = SUBCHUNK_SIZE;
			
			bid = subchunk & ~SUBC_SPARSE_BIT;
		} else {
			if ((stepmask & ~SUBCHUNK_MASK) != 0) {
				// TODO: projection has precision problem when hitting edge exactly I think (white dots in iteration visualization) -> inf loop
				#if 1
				vec3 proj = ray.pos + ray.dir * dist;
				//proj += mix(vec3(0.0), sign(ray.dir) * 0.5, axis_mask);
				if (dist > 0.0)
					proj[axis] += sign(ray.dir[axis]) * 0.5;
				vec3 coordf = floor(proj);
				#else
				vec3 coordf = floor(ray.pos + ray.dir * (dist + 0.001));
				#endif
				
				coord = ivec3(coordf);
			}
			stepsize = 1;
			
			ivec3 subc_offs = subchunk_id_to_texcoords(subchunk);
			bid = texelFetch(subchunk_voxels, subc_offs + (coord & SUBCHUNK_MASK), 0).r;
		}
		
		if (hit_voxel(bid, prev_bid, ray.pos, ray.dir, dist, axis, hit_col))
			return true;
		prev_bid = bid;
		
		if (++iterations >= max_iterations || dist >= ray.max_dist)
			return false; // max dist reached
		
		vec3 next;
		{
			ivec3 planecoord = coord + ivec3(stepsize) * ivec3((~floatBitsToUint(ray.dir)) >> 31);
			next = rdir * abs(ray.pos - vec3(planecoord));
		}
		
		dist = min(min(next.x, next.y), next.z);
		
		if (next.x == dist) {
			axis = 0;
			
			int old_coord = coord.x;
			coord.x += (ray.dir.x >= 0.0 ? 1 : -1) * stepsize;
			next.x  += rdir.x * float(stepsize);
			
			stepmask = old_coord ^ coord.x;
		} else if (next.y == dist) {
			axis = 1;
			
			int old_coord = coord.y;
			coord.y += (ray.dir.y >= 0.0 ? 1 : -1) * stepsize;
			next.y  += rdir.y * float(stepsize);
			
			stepmask = old_coord ^ coord.y;
		} else {
			axis = 2;
			
			int old_coord = coord.z;
			coord.z += (ray.dir.z >= 0.0 ? 1 : -1) * stepsize;
			next.z  += rdir.z * float(stepsize);
			
			stepmask = old_coord ^ coord.z;
		}
		
		if ((stepmask & ~CHUNK_SIZE_MASK) != 0) {
			// stepped out of chunk
			ray.chunkid = get_neighbour(ray.chunkid, get_step_face(axis, ray.dir) ^ 1);
			if (ray.chunkid == 0xffffu)
				return false;
		}
	}
	
	return false;
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
	
	Ray ray;
	get_ray(pos, ray.chunkid, ray.pos, ray.dir);
	ray.max_dist = INF;
		
	vec3 col;
	if (!trace_ray(ray, col))
		col = vec3(0.0);
	
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
	imageStore(img, ivec2(pos), vec4(col, 1.0));
}
