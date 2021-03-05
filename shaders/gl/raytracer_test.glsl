#version 460 core
//#extension GL_ARB_gpu_shader5 : enable
//#extension GL_EXT_shader_16bit_storage : enable // not supported
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_ARB_shader_group_vote : enable

#include "common.glsl"


//// https://stackoverflow.com/questions/4200224/random-noise-functions-for-glsl
// Gold Noise ©2015 dcerisano@standard3d.com
// - based on the Golden Ratio
// - uniform normalized distribution
// - fastest static noise generator function (also runs at low precision)

float PHI = 1.61803398874989484820459;  // Φ = Golden Ratio   

float gold_noise(in vec2 xy, in float seed){
	return fract(tan(distance(xy*PHI, xy)*seed)*xy.x);
}
///

uniform float time = 0.0;

float seed = time + 1.0;

float rand () {
	return gold_noise(gl_GlobalInvocationID.xy, seed++);
}
vec2 rand2 () {
	return vec2(rand(), rand());
}
vec3 rand3 () {
	return vec3(rand(), rand(), rand());
}

#define PI	3.1415926535897932384626433832795

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
vec3 get_bounce_dir (vec3 normal) {
	mat3 tangent_to_world;
	{
		vec3 tangent = abs(normal.x) >= 0.98 ? vec3(0,1,0) : vec3(1,0,0);
		vec3 bitangent = cross(normal, tangent);

		tangent_to_world = mat3(tangent, bitangent, normal);
	}

	return tangent_to_world * hemisphere_sample();
}

layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y) in;

#define CHUNK_SIZE			64 // size of chunk in blocks per axis
#define CHUNK_SIZE_SHIFT	6 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		63

#define SUBCHUNK_SIZE		4 // size of subchunk in blocks per axis
#define SUBCHUNK_COUNT		16 // size of chunk in subchunks per axis
#define SUBCHUNK_SHIFT		2
#define SUBCHUNK_MASK		3

#define CHUNK_SPARSE_VOXELS	 4


#define B_AIR 1
#define B_WATER 3
#define B_MAGMA 12
#define B_CRYSTAL 15
#define B_URANIUM 16

float get_alpha_mult (uint bid) {
	if (      bid == B_WATER   ) return 0.08;
	else if ( bid == B_CRYSTAL ) return 0.22;
	return 1.0;
}
float get_emmisive_mult (uint bid) {
	if (      bid == B_MAGMA   ) return 40.0;
	else if ( bid == B_CRYSTAL ) return 30.0;
	else if ( bid == B_URANIUM ) return 60.0;
	return 0.0;
}

struct Chunk {
	uint flags;
	//ivec3 pos;
	int posx; int posy; int posz;

	//uint16_t neighbours[6];
	uint _neighbours[3];

	//uint16_t voxel_data; // if SPARSE_VOXELS: non-null block id   if !SPARSE_VOXELS: id to dense_chunks
	//uint16_t _pad;

	uint _voxel_data;

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

layout(rgba16f, binding = 3) uniform image2D img;

layout(std430, binding = 4) readonly buffer Chunks {
	Chunk chunks[];
};
layout(std430, binding = 5) readonly buffer DenseChunks {
	ChunkVoxels dense_chunks[];
};
layout(std430, binding = 6) readonly buffer DenseSubchunks {
	SubchunkVoxels dense_subchunks[];
};

//
bool chunk_sparse (uint cid) {
	uint test = (chunks[cid].flags & CHUNK_SPARSE_VOXELS);
	return test != 0u;
}
ivec3 get_pos (uint cid) {
	return ivec3(chunks[cid].posx, chunks[cid].posy, chunks[cid].posz);
}
uint get_voxel_data (uint cid) {
	return chunks[cid]._voxel_data & 0xffffu;
}
uint get_neighbour (uint cid, int i) {
	return (chunks[cid]._neighbours[i >> 1] >> ((i & 1) * 16)) & 0xffffu;
}

int get_subchunk_idx (ivec3 pos) {
	pos &= CHUNK_SIZE_MASK;
	pos >>= SUBCHUNK_SHIFT;
	return pos.z * SUBCHUNK_COUNT*SUBCHUNK_COUNT + pos.y * SUBCHUNK_COUNT + pos.x;
}
bool is_subchunk_sparse (uint dc_id, int subc_i) {
	uint test = dense_chunks[dc_id].sparse_bits[subc_i >> 5] & (1u << (subc_i & 31));
	return test != 0u;
}

uint get_voxel (uint subc_id, ivec3 pos) {
	pos &= SUBCHUNK_MASK;
	uint val = dense_subchunks[subc_id].voxels[pos.z][pos.y][pos.x >> 1];
	return (val >> ((pos.x & 1) * 16)) & 0xffffu;
}

uniform uint camera_chunk;

uniform sampler2DArray	tile_textures;

// get pixel ray in world space based on pixel coord and matricies
void get_ray (vec2 px_pos, out vec3 ray_pos, out vec3 ray_dir) {

	//vec2 px_jitter = rand2(gl_FragCoord.xy) - 0.5;
	vec2 px_jitter = vec2(0.0);

	vec2 ndc = (px_pos + 0.5 + px_jitter) / view.viewport_size * 2.0 - 1.0;
	vec4 clip = vec4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;

	// TODO: can't get optimization to one single clip_to_world mat mul to work, why?
	vec3 cam = (view.clip_to_cam * clip).xyz;

	ray_pos = (view.cam_to_world * vec4(cam, 1)).xyz;
	ray_dir = (view.cam_to_world * vec4(cam, 0)).xyz;
	ray_dir = normalize(ray_dir);
}

const float INF = 1. / 0.;

int find_next_axis (vec3 next) { // index of smallest component
	if (		next.x < next.y && next.x < next.z )	return 0;
	else if (	next.y < next.z )						return 1;
	else												return 2;
}
int get_step_face (int axis, vec3 ray_dir) {
	return axis*2 +(ray_dir[axis] >= 0.0 ? 0 : 1);
}

const float max_dist = 100.0;
uniform int max_iterations = 200;

vec4 accum_col = vec4(0,0,0,0);

void add_color (vec4 col) {
	accum_col += vec4(col.rgb * col.a, col.a) * (1.0 - accum_col.a);
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

uint chunk_id;
uint voxel_data;
uint subchunk_data;

#if VISUALIZE_COST
	#if VISUALIZE_WARP_COST && VISUALIZE_WARP_READS
		#define WARP_COUNT_IN_WG 2
		shared uint warp_reads[WARP_COUNT_IN_WG][2];
		uint local_reads[2];
	#endif

	uniform sampler2D		heat_gradient;
#endif

uint read_voxel (int step_mask, inout ivec3 coord, out float step_size) {
	if ((step_mask & ~CHUNK_SIZE_MASK) != 0) {

	#if VISUALIZE_COST && VISUALIZE_WARP_COST && VISUALIZE_WARP_READS
		local_reads[0]++;
		if (subgroupElect())
			atomicAdd(warp_reads[gl_SubgroupID][0], 1u);
	#endif
		
		voxel_data = get_voxel_data(chunk_id);
		if (chunk_sparse(chunk_id)) {

			coord &= ~CHUNK_SIZE_MASK;

			step_size = float(CHUNK_SIZE);
			return voxel_data;
		}
	}

	if ((step_mask & ~SUBCHUNK_MASK) != 0) {

	#if VISUALIZE_COST && VISUALIZE_WARP_COST && VISUALIZE_WARP_READS
		local_reads[1]++;
		if (subgroupElect())
			atomicAdd(warp_reads[gl_SubgroupID][1], 1u);
	#endif

		int subci = get_subchunk_idx(coord);

		subchunk_data = dense_chunks[voxel_data].sparse_data[subci];
		if (is_subchunk_sparse(voxel_data, subci)) {

			coord &= ~SUBCHUNK_MASK;

			step_size = float(SUBCHUNK_SIZE);
			return subchunk_data;
		}
	}

	step_size = 1.0;
	return get_voxel(subchunk_data, coord);
}

int iterations = 0;

vec3 hit_pos;
vec3 hit_normal;
float hit_emiss;

bool hit_voxel (uint bid, vec3 proj, int axis, vec3 ray_dir, float step_size) {
	if (bid == B_AIR)
		return false;

	hit_pos = proj;

	int entry_face = get_step_face(axis, ray_dir);
	
	vec2 uv = calc_uv(fract(proj), entry_face);

	vec3 normal = vec3(0.0);
	normal[axis] = -sign(ray_dir[axis]);
	
	hit_normal = normal;

	float texid = float(block_tiles[bid].sides[entry_face]);

	vec4 col = textureLod(tile_textures, vec3(uv, texid), bid == B_WATER ? 20.0 : 0.0);
	if (col.a < 0.5)
		return false;
	
	col.a *= get_alpha_mult(bid);
	hit_emiss = get_emmisive_mult(bid);
	
	col.a *= step_size; // try to account for large steps through subchunks
	col.a = min(col.a, 1.0);
	add_color(col);
	return accum_col.a > 0.95;
}

bool trace_ray (vec3 ray_pos, vec3 ray_dir) {
	ivec3 coord = ivec3(floor(ray_pos));

	vec3 rdir; // reciprocal of ray dir
	rdir.x = ray_dir.x != 0.0 ? 1.0 / abs(ray_dir.x) : INF;
	rdir.y = ray_dir.y != 0.0 ? 1.0 / abs(ray_dir.y) : INF;
	rdir.z = ray_dir.z != 0.0 ? 1.0 / abs(ray_dir.z) : INF;

	int axis = 0;
	vec3 proj = ray_pos + ray_dir * 0;

	int step_mask = -1;
	float dist = 0.0;

	while (iterations < max_iterations) {
		iterations++;

		float step_size;
		uint bid = read_voxel(step_mask, coord, step_size);

		if (hit_voxel(bid, proj, axis, ray_dir, step_size))
			return true;

		vec3 rel = ray_pos - vec3(coord);
		
		vec3 plane_offs;
		plane_offs.x = ray_dir.x >= 0.0 ? float(step_size.x) - rel.x : rel.x;
		plane_offs.y = ray_dir.y >= 0.0 ? float(step_size.x) - rel.y : rel.y;
		plane_offs.z = ray_dir.z >= 0.0 ? float(step_size.x) - rel.z : rel.z;

		vec3 next = rdir * plane_offs;
		axis = find_next_axis(next);

		// fix infinite loop caused by zero-length step caused by stepping exactly onto voxel edge
		// this causes visual artefact that is less bad, but becomes noticable at large min step dist
		// this min_step_dist needs to be larger and larger the further we get from the floating point origin
		// so it is advisable to keep ray_pos relative to the starting chunk
		float min_step_dist = 0.0001;
		proj = ray_pos + ray_dir * (dist + max(next[axis] - dist, min_step_dist));
		
		dist = next[axis];

		//if (next[axis] > max_dist)
		//	break;

		ivec3 old_coord = coord;
		
		vec3 tmp = proj;
		tmp[axis] += ray_dir[axis] >= 0 ? 0.5 : -0.5;
		coord = ivec3(floor(tmp));
		
		ivec3 step_mask3 = coord ^ old_coord;
		step_mask = step_mask3.x | step_mask3.y | step_mask3.z;
		
		// handle step out of chunk by checking bits
		if ((step_mask & ~CHUNK_SIZE_MASK) != 0) {
			chunk_id = get_neighbour(chunk_id, get_step_face(axis, ray_dir) ^ 1); // ^1 flip dir
			if (chunk_id == 0xffffu)
				break;
		}
	}

	return false;
}

void main () {
#if VISUALIZE_COST && VISUALIZE_WARP_COST && VISUALIZE_WARP_READS
	if (subgroupElect()) {
		warp_reads[gl_SubgroupID][0] = 0u;
		warp_reads[gl_SubgroupID][1] = 0u;
	}
	local_reads[0] = 0u;
	local_reads[1] = 0u;

	barrier();
#endif
	vec2 pos = gl_GlobalInvocationID.xy;

	chunk_id = camera_chunk;

	vec3 ray_pos, ray_dir;
	get_ray(pos, ray_pos, ray_dir);

	// primary ray
	if (trace_ray(ray_pos, ray_dir)) {
		
		vec4 albedo = accum_col;
		vec4 accum = vec4(0.0);

		// secondary rays
		int rays = 16;
		for (int i=0; i<rays; ++i) {
			ray_pos = hit_pos + hit_normal * 0.00001;
			//ray_dir = reflect(ray_dir, hit_normal);
			ray_dir = get_bounce_dir(hit_normal);
		
			accum_col = vec4(0.0);
			trace_ray(ray_pos, ray_dir);

			accum += accum_col * hit_emiss;
		}
		
		accum_col = (accum/float(rays) + 0.02) * albedo;
	}

#if VISUALIZE_COST
	#if VISUALIZE_WARP_COST
	#if VISUALIZE_WARP_READS
		const uint warp_cost = warp_reads[gl_SubgroupID][0] + warp_reads[gl_SubgroupID][1];
		const uint local_cost = local_reads[0] + local_reads[1];
	#else
		const uint warp_cost = subgroupMax(iterations);
		const uint local_cost = iterations;
	#endif
		float wasted_work = float(warp_cost - local_cost) / float(warp_cost);
		accum_col = texture(heat_gradient, vec2(wasted_work, 0.5));
	#else
		accum_col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5));
	#endif
#endif

	// maybe try not to do rays that we do not see (happens due to local group size)
	if (pos.x < view.viewport_size.x && pos.y < view.viewport_size.y)
		imageStore(img, ivec2(pos), accum_col);
}
