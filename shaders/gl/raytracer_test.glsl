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

#define B_AIR 1

uniform uint camera_chunk;

uniform sampler2DArray	tile_textures;

#if VISUALIZE_ITERATIONS
uniform sampler2D		heat_gradient;
#endif

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

uint read_voxel (uint chunk_id, inout ivec3 coord, out float step_size) {
	ivec3 co = coord % 64;

	uint chunk_voxdat = get_voxel_data(chunk_id);
	if (chunk_sparse(chunk_id)) {

		coord &= ~CHUNK_SIZE_MASK;

		step_size = float(CHUNK_SIZE);
		return chunk_voxdat;
	}

	int subci = get_subchunk_idx(co);

	uint sparse_data = dense_chunks[chunk_voxdat].sparse_data[subci];
	if (is_subchunk_sparse(chunk_voxdat, subci)) {

		coord &= ~SUBCHUNK_MASK;

		step_size = float(SUBCHUNK_SIZE);
		return sparse_data;
	}

	step_size = 1.0;
	return get_voxel(sparse_data, co);
}

bool hit_voxel (uint bid, vec3 proj, int axis, vec3 ray_dir) {
	if (bid == B_AIR)
		return false;

	int entry_face = get_step_face(axis, ray_dir);
	
	vec2 uv = calc_uv(fract(proj), entry_face);
	float texid = float(block_tiles[bid].sides[entry_face]);

	vec4 col = texture(tile_textures, vec3(uv, texid));
	if (col.a == 0.0)
		return false;

	hit_col = col;
	return true;
}

void trace_pixel (vec2 px_pos) {
	vec3 ray_pos, ray_dir;
	get_ray(px_pos, ray_pos, ray_dir);

	uint chunk_id = camera_chunk;

	// voxel ray stepping setup

	ivec3 coord = ivec3(floor(ray_pos));

	// get how far you have to travel along the ray to move by 1 unit in each axis
	// (ray_dir / abs(ray_dir.x) normalizes the ray_dir so that its x is 1 or -1
	// a zero in ray_dir produces a NaN in step because 0 / 0
	vec3 step_dist;
	step_dist.x = length(ray_dir / abs(ray_dir.x));
	step_dist.y = length(ray_dir / abs(ray_dir.y));
	step_dist.z = length(ray_dir / abs(ray_dir.z));

	step_dist = mix(step_dist, vec3(INF), equal(ray_dir, vec3(0.0)));

	int axis = 0;
	vec3 proj = ray_pos + ray_dir * 0;

	int iterations = 0;
	while (iterations < max_iterations) {
		iterations++;

		float step_size;
		uint bid = read_voxel(chunk_id, coord, step_size);

		if (hit_voxel(bid, proj, axis, ray_dir))
			break;

		vec3 rel = ray_pos - vec3(coord);
		vec3 plane_offs = mix(vec3(step_size) - rel, rel, step(ray_dir, vec3(0.0)));

		vec3 next = step_dist * plane_offs;
		axis = find_next_axis(next);

		proj = ray_pos + ray_dir * next[axis];

		//if (next[axis] > max_dist)
		//	break;

		ivec3 old_coord = coord;

		proj[axis] += sign(ray_dir[axis]) * 0.5f;
		coord = ivec3(floor(proj));

		ivec3 step_mask = coord ^ old_coord;
		// handle step out of chunk by checking bits
		if (any((step_mask & ~ivec3(CHUNK_SIZE_MASK)) != 0)) {
			chunk_id = get_neighbour(chunk_id, get_step_face(axis, ray_dir) ^ 1); // ^1 flip dir
			if (chunk_id == 0xffffu)
				return;
		}
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
