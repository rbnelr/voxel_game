#version 460 core // for GL_ARB_shader_draw_parameters
//#extension GL_ARB_gpu_shader5 : enable
//#extension GL_EXT_shader_16bit_storage : enable // not supported

#include "common.glsl"

layout(local_size_x = LOCAL_SIZE, local_size_y = LOCAL_SIZE) in;

#define CHUNK_SIZE			64 // size of chunk in blocks per axis
#define CHUNK_SIZE_SHIFT	6 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		63

#define SUBCHUNK_SIZE		4 // size of subchunk in blocks per axis
#define SUBCHUNK_COUNT		16 // size of chunk in subchunks per axis
#define SUBCHUNK_SHIFT		2
#define SUBCHUNK_MASK		3

#define CHUNK_SPARSE_VOXELS	 4

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
	pos >>= SUBCHUNK_SHIFT;
	return pos.z * SUBCHUNK_COUNT*SUBCHUNK_COUNT + pos.y * SUBCHUNK_COUNT + pos.x;
}
bool is_subchunk_sparse (uint dc_id, int subc_i) {
	uint test = dense_chunks[dc_id].sparse_bits[subc_i >> 5] & (1u << (subc_i & 31));
	return test != 0u;
}

uint get_voxel (uint subc_id, ivec3 pos) {
	ivec3 masked = pos & ivec3(SUBCHUNK_MASK);
	uint val = dense_subchunks[subc_id].voxels[masked.z][masked.y][masked.x >> 1];
	return (val >> ((masked.x & 1) * 16)) & 0xffffu;
}

#define B_AIR 1

uniform uint camera_chunk;

uniform sampler2DArray	tile_textures;

#if VISUALIZE_ITERATIONS
uniform sampler2D		heat_gradient;
#endif

vec3 ray_pos, ray_dir;

// get pixel ray in world space based on pixel coord and matricies
void get_ray (vec2 px_pos) {

	//vec2 px_jitter = rand2(gl_FragCoord.xy) - 0.5;
	vec2 px_jitter = vec2(0.0);

	vec2 ndc = (px_pos + 0.5 + px_jitter) / view.viewport_size * 2.0 - 1.0;
	vec4 clip = vec4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;

	// TODO: can't get optimization to one single clip_to_world mat mul to work, why?
	vec3 cam = (view.clip_to_cam * clip).xyz;

	ray_pos = (view.cam_to_world * vec4(cam, 1)).xyz;
	ray_dir = (view.cam_to_world * vec4(cam, 0)).xyz;
	ray_dir = normalize(ray_dir);

	//DEBUG(vec4(ray_dir, 1));
	//ray_pos -= vec3(svo_root_pos);
}

const float INF = 1. / 0.;

int find_next_axis (vec3 next) { // index of smallest component
	if (		next.x < next.y && next.x < next.z )	return 0;
	else if (	next.y < next.z )						return 1;
	else												return 2;
}

int get_step_face (int cur_axis, vec3 step_delta) {
	return cur_axis*2 +(step_delta[cur_axis] >= 0 ? 1 : 0);
}

const float max_dist = 100.0;

uniform int max_iterations = 200;

uint chunk_id;

ivec3	step_delta = ivec3(0); // shut up about might be uninitialized
vec3	step_dist;
vec3	next;
ivec3	cur_voxel;

int		cur_axis;
float	cur_dist;

vec4 hit_col = vec4(0,0,0,0);

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

bool hit_voxel () {
	// handle step out of chunk by checking bits
	if ((cur_voxel[cur_axis] & ~CHUNK_SIZE_MASK) != 0) {
		chunk_id = get_neighbour(chunk_id, get_step_face(cur_axis, step_delta));
		if (chunk_id == 0xffffu)
			return true;

		cur_voxel &= CHUNK_SIZE_MASK; // make pos chunk relative again
	}
	
	// read voxel
	uint bid = 0;
	{
		int step_size = 1;
		
		uint chunk_voxdat = get_voxel_data(chunk_id);
		if (chunk_sparse(chunk_id)) {

			bid = chunk_voxdat;
			//step_size = CHUNK_SIZE;
		} else {

			int subci = get_subchunk_idx(cur_voxel);
			
			uint sparse_data = dense_chunks[chunk_voxdat].sparse_data[subci];
			if (is_subchunk_sparse(chunk_voxdat, subci)) {

				bid = sparse_data;
				//step_size = SUBCHUNK_SIZE;
			} else {

				bid = get_voxel(sparse_data, cur_voxel);
			}
		}

		step_delta = ivec3(sign(ray_dir)) * step_size;
	}
	
	if (bid == B_AIR)
		return false;

	int entry_face = get_step_face(cur_axis, step_delta);
	
	vec3 hit_pos_world = ray_pos + ray_dir * cur_dist;
	
	vec2 uv = calc_uv(fract(hit_pos_world), entry_face);
	float texid = float(block_tiles[bid].sides[entry_face]);

	vec4 col = texture(tile_textures, vec3(uv, texid));
	if (col.a == 0.0)
		return false;

	hit_col = col;
	return true;
}

void trace_pixel (vec2 px_pos) {
	get_ray(px_pos);

	chunk_id = camera_chunk;
	vec3 chunk_pos = vec3(get_pos(chunk_id) * float(CHUNK_SIZE));
	ray_pos -= chunk_pos;

	// voxel ray stepping setup
	vec3 abs_dir = abs(ray_dir);
	step_dist.x = length(ray_dir / abs_dir.x);
	step_dist.y = length(ray_dir / abs_dir.y);
	step_dist.z = length(ray_dir / abs_dir.z);

	vec3 pos_in_block = fract(ray_pos);

	cur_voxel = ivec3(floor(ray_pos));

	next = step_dist * mix( 1.0 - pos_in_block, pos_in_block, step(ray_dir, vec3(0.0)) );

	// NaN -> Inf
	next = mix(next, vec3(INF), equal(ray_dir, vec3(0.0)));

	// find the axis of the cur step
	cur_axis = find_next_axis(next);
	cur_dist = next[cur_axis];

	int iterations = 0;
	while (iterations < max_iterations && !hit_voxel()) {
		iterations++;

		// find the axis of the cur step
		cur_axis = find_next_axis(next);
		cur_dist = next[cur_axis];
		
		// clac the distance at which the next voxel step for this axis happens
		next[cur_axis] += step_dist[cur_axis];
		// step into the next voxel
		cur_voxel[cur_axis] += step_delta[cur_axis];
	}

#if VISUALIZE_ITERATIONS
	hit_col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5));
#endif
}

void main () {
	vec2 pos = gl_GlobalInvocationID.xy;

	// maybe try not to do rays that we do not see (happens due to local group size)
	if (pos.x >= view.viewport_size.x || pos.y >= view.viewport_size.y)
		return;

	trace_pixel(pos);

	if (pos.x > 200)
		imageStore(img, ivec2(pos), hit_col);
}
