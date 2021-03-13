#version 460 core
//#extension GL_ARB_gpu_shader5 : enable
//#extension GL_EXT_shader_16bit_storage : enable // not supported
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_ARB_shader_group_vote : enable

layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y) in;

#include "common.glsl"
#include "rand.glsl"

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

float get_emmisive_mult (uint bid) {
	if (      bid == B_MAGMA   ) return 40.0;
	else if ( bid == B_CRYSTAL ) return 10.0;
	else if ( bid == B_URANIUM ) return 20.0;
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
	pos &= CHUNK_SIZE_MASK;
	pos >>= SUBCHUNK_SHIFT;
	return pos.z * SUBCHUNK_COUNT*SUBCHUNK_COUNT + pos.y * SUBCHUNK_COUNT + pos.x;
}
bool is_subchunk_sparse (uint cid, int subc_i) {
	uint test = chunk_voxels[cid].sparse_bits[subc_i >> 5] & (1u << (subc_i & 31));
	return test != 0u;
}

uint get_voxel (uint subc_id, ivec3 pos) {
	pos &= SUBCHUNK_MASK;
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
uniform int   sunlight_rays = 2;
uniform float sunlight_dist = 90.0;
uniform vec3  sunlight_col = vec3(0.98, 0.92, 0.65) * 1.3;

uniform bool  ambient_enable = true;
uniform int   ambient_rays = 2;
uniform float ambient_dist = 10.0;
uniform vec3  ambient_col = vec3(0.5, 0.8, 1.0) * 0.8;

uniform bool  bouncelight_enable = true;
uniform int   bouncelight_rays = 2;
uniform float bouncelight_dist = 30.0;

uniform bool  visualize_light = false;

// get pixel ray in world space based on pixel coord and matricies
void get_ray (vec2 px_pos, out vec3 ray_pos, out vec3 ray_dir) {

	//vec2 px_jitter = rand2(gl_FragCoord.xy) - 0.5;
	vec2 px_jitter = vec2(0.0);

	vec2 ndc = (px_pos + 0.5 + px_jitter) / view.viewport_size * 2.0 - 1.0;
	vec4 clip = vec4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;

	// TODO: can't get optimization to one single clip_to_world mat mul to work, why?
	vec3 cam = (view.clip_to_cam * clip).xyz;

	ray_dir = (view.cam_to_world * vec4(cam, 0)).xyz;
	ray_dir = normalize(ray_dir);
	
	// ray starts on the near plane
	//ray_pos = (view.cam_to_world * vec4(cam, 1)).xyz;
	
	// all rays starts exactly on the camera
	ray_pos = (view.cam_to_world * vec4(0,0,0,1)).xyz;
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

int iterations = 0;

struct Hit {
	vec3	pos;
	vec3	normal;
	uint	chunkid;
	
	vec4	col;
};

bool hit_voxel (uint bid, uint prev_bid, vec3 ray_pos, vec3 ray_dir, float dist, int axis, inout Hit hit) {
	if (prev_bid == bid && (bid != B_LEAVES && bid != B_TALLGRASS))
		return false;
	if (bid == B_AIR)
		bid = prev_bid;
	
	hit.pos = ray_pos + ray_dir * dist;
	
	int entry_face = get_step_face(axis, ray_dir);
	
	vec2 uv = calc_uv(fract(hit.pos), entry_face);

	vec3 normal = vec3(0.0);
	normal[axis] = -sign(ray_dir[axis]);
	
	hit.normal = normal;

	float texid = float(block_tiles[bid].sides[entry_face]);
	
	vec4 col = texture(tile_textures, vec3(uv, texid));
	
	if (bid == B_TALLGRASS && axis == 2)
		col = vec4(0.0);
	
	// accum color
	col.a *= 1.0 - hit.col.a;
	
	hit.col.rgb += col.rgb * col.a;
	hit.col.a += col.a;
	
	return hit.col.a > 0.99;
}

bool trace_ray (uint chunkid, vec3 ray_pos, vec3 ray_dir, float max_dist, inout Hit hit) {
	vec3 floorpos = floor(ray_pos);
	ivec3 coord = ivec3(floorpos);
	
	vec3 rdir; // reciprocal of ray dir
	rdir.x = ray_dir.x != 0.0 ? 1.0 / abs(ray_dir.x) : INF;
	rdir.y = ray_dir.y != 0.0 ? 1.0 / abs(ray_dir.y) : INF;
	rdir.z = ray_dir.z != 0.0 ? 1.0 / abs(ray_dir.z) : INF;

	vec3 next;
	{
		vec3 rel = ray_pos - floorpos;
		next = rdir * mix(1.0 - rel, rel, step(ray_dir, vec3(0.0)));
	}
	vec3 subc_next;
	{
		vec3 rel = ray_pos - floor(ray_pos / 8.0) * 8.0;
		subc_next = rdir * mix(8.0 - rel, rel, step(ray_dir, vec3(0.0)));
	}
	
	float dist = min(min(next.x, next.y), next.z);
	bvec3 axes = equal(vec3(dist), next);
	
	uint prev_bid = 0;
	while (chunkid != 0xffffu) { // for chunks
		
		coord &= ivec3(CHUNK_SIZE_MASK);
		
		for (;;) { // for subchunks
			
			float subc_dist = min(min(subc_next.x, subc_next.y), subc_next.z);
			bvec3 subc_axes = equal(vec3(subc_dist), subc_next);
			
			
			int subci = get_subchunk_idx(coord);
			
			uint subchunk_data = chunk_voxels[chunkid].sparse_data[subci];
			if (is_subchunk_sparse(chunkid, subci)) {
				
				if (subc_dist > max_dist || iterations++ >= max_iterations)
					return false;
				
				uint bid = subchunk_data;
				
				if (prev_bid == 0) prev_bid = bid;
				if (hit_voxel(bid, prev_bid, ray_pos, ray_dir, subc_dist, get_axis(subc_axes), hit))
					return true;
				prev_bid = bid;
				
				vec3 steps;
				if (     next.x + rdir.x*0.0 >= subc_dist - 0.01) steps.x = 0.0;
				else if (next.x + rdir.x*1.0 >= subc_dist - 0.01) steps.x = 1.0;
				else if (next.x + rdir.x*2.0 >= subc_dist - 0.01) steps.x = 2.0;
				else if (next.x + rdir.x*3.0 >= subc_dist - 0.01) steps.x = 3.0;
				else if (next.x + rdir.x*4.0 >= subc_dist - 0.01) steps.x = 4.0;
				
				if (     next.y + rdir.y*0.0 >= subc_dist - 0.01) steps.y = 0.0;
				else if (next.y + rdir.y*1.0 >= subc_dist - 0.01) steps.y = 1.0;
				else if (next.y + rdir.y*2.0 >= subc_dist - 0.01) steps.y = 2.0;
				else if (next.y + rdir.y*3.0 >= subc_dist - 0.01) steps.y = 3.0;
				else if (next.y + rdir.y*4.0 >= subc_dist - 0.01) steps.y = 4.0;
				
				if (     next.z + rdir.z*0.0 >= subc_dist - 0.01) steps.z = 0.0;
				else if (next.z + rdir.z*1.0 >= subc_dist - 0.01) steps.z = 1.0;
				else if (next.z + rdir.z*2.0 >= subc_dist - 0.01) steps.z = 2.0;
				else if (next.z + rdir.z*3.0 >= subc_dist - 0.01) steps.z = 3.0;
				else if (next.z + rdir.z*4.0 >= subc_dist - 0.01) steps.z = 4.0;
				
				//vec3 steps = floor( (vec3(subc_dist) -next) / rdir );
				axes = subc_axes;
				
				next += rdir * steps;
				coord += ivec3(sign(ray_dir) * steps);
				
				//dist = min(min(next.x, next.y), next.z);
				//axes = equal(vec3(dist), next);
				//
				//next += mix(vec3(0.0), rdir, axes);
				//coord += mix(ivec3(0), ivec3(sign(ray_dir)), axes);
				
			} else {
				
				for (;;) { // for voxels
					
					if (dist > max_dist || iterations++ >= max_iterations)
						return false;
					
					uint bid = get_voxel(subchunk_data, coord);
					
					if (prev_bid == 0) prev_bid = bid;
					if (hit_voxel(bid, prev_bid, ray_pos, ray_dir, dist, get_axis(axes), hit))
						return true;
					prev_bid = bid;
					
					dist = min(min(next.x, next.y), next.z);
					axes = equal(vec3(dist), next);
					
					next += mix(vec3(0.0), rdir, axes);
					coord += mix(ivec3(0), ivec3(sign(ray_dir)), axes);
					
					if (dist >= subc_dist)
						break;
				}
			}
			
			subc_next += mix(vec3(0.0), rdir, subc_axes) * 8.0;
			
			int step_mask = coord.x | coord.y | coord.z;
			if ((step_mask & ~CHUNK_SIZE_MASK) != 0)
				break;
		}
		
		return false;
		//// step to next chunk
		//chunkid = get_neighbour(chunkid, get_step_face(axis, ray_dir) ^ 1); // ^1 flip dir
	}
	
	return false;
}

/*
	
			#if VISUALIZE_COST && VISUALIZE_WARP_COST
				if (subgroupElect())
					atomicAdd(warp_iter[gl_SubgroupID], 1u);
			#endif
				
*/

vec3 tonemap (vec3 c) {
	
	//c *= 2.0;
	//c = c / (c + 1.0);
	
	//c *= 0.2;
	//vec3 x = max(vec3(0.0), c -0.004);
	//c = (x*(6.2*x+.5))/(x*(6.2*x+1.7)+0.06);
	return c;
}

void main () {
#if VISUALIZE_COST && VISUALIZE_WARP_COST
	if (subgroupElect()) {
		warp_iter[gl_SubgroupID] = 0u;
	}

	barrier();
#endif

	vec2 pos = gl_GlobalInvocationID.xy;
	
	vec3 ray_pos, ray_dir;
	get_ray(pos, ray_pos, ray_dir);

	// primary ray
	Hit hit;
	hit.col = vec4(0.0);
	//hit.emiss = vec3(0.0);
	if (trace_ray(camera_chunk, ray_pos, ray_dir, INF, hit)) {
	//	
	//	vec3 bounce_pos = hit.pos + hit.normal * 0.001;
	//	
	//	
	//	// secondary rays
	//	
	//	vec3 light = vec3(0.00);
	//	
	//	if (sunlight_enable) {
	//		// fake directional light
	//		float accum = 0.0;
	//		for (int i=0; i<sunlight_rays; ++i) {
	//			ray_dir = normalize(vec3(1,2,3)) + rand3()*0.05;
	//	
	//			Hit hit2;
	//			hit2.col = vec4(0.0);
	//			hit2.emiss = vec3(0.0);
	//			if (trace_ray(hit.chunkid, bounce_pos, ray_dir, sunlight_dist, hit2))
	//				accum += hit2.col.a;
	//		}
	//	
	//		light += (1.0 - accum / float(sunlight_rays)) * sunlight_col;
	//	}
	//	
	//	float ambient = 0.5;
	//	
	//	if (ambient_enable) {
	//		
	//		// hemisphere AO
	//		float accum = 0.0;
	//		
	//		for (int i=0; i<ambient_rays; ++i) {
	//			Hit hit2;
	//			hit2.col = vec4(0.0);
	//			hit2.emiss = vec3(0.0);
	//			if (trace_ray(hit.chunkid, bounce_pos, get_bounce_dir(hit.normal), ambient_dist, hit2)) {
	//				accum += clamp(hit2.col.a, 0.0, 1.0);
	//			}
	//		}
	//		
	//		ambient = pow(1.0 - accum / float(ambient_rays), 1.5);
	//	}
	//	
	//	light += ambient_col * ambient;
	//	
	//	if (bouncelight_enable) {
	//		// Lighting in 64 block radius from emissive blocks
	//		vec3 accum = vec3(0.0);
	//		for (int i=0; i<bouncelight_rays; ++i) {
	//			Hit hit2;
	//			hit2.col = vec4(0.0);
	//			hit2.emiss = vec3(0.0);
	//			trace_ray(hit.chunkid, bounce_pos, get_bounce_dir(hit.normal), bouncelight_dist, hit2);
	//			
	//			accum += hit2.emiss;
	//		}
	//		
	//		light += accum / float(bouncelight_rays);
	//	}
	//	
	//	hit.col.rgb *= light;
	//	
	//	if (visualize_light)
	//		hit.col.rgb = vec3(light);
	}

#if VISUALIZE_COST
	#if VISUALIZE_WARP_COST
		const uint warp_cost = warp_iter[gl_SubgroupID];
		const uint local_cost = iterations;
		
		float wasted_work = float(warp_cost - local_cost) / float(warp_cost);
		hit.col = texture(heat_gradient, vec2(wasted_work, 0.5));
	#else
		hit.col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5));
	#endif
#endif
	
	hit.col.rgb = tonemap(hit.col.rgb);
	
	// maybe try not to do rays that we do not see (happens due to local group size)
	if (pos.x < view.viewport_size.x && pos.y < view.viewport_size.y)
		imageStore(img, ivec2(pos), hit.col);
}
