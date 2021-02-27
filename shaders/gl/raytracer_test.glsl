#version 460 core // for GL_ARB_shader_draw_parameters
#extension GL_NV_gpu_shader5 : enable
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
	uint32_t flags;
	//ivec3 pos;
	int posx; int posy; int posz;

	//uint16_t neighbours[6];
	uint32_t _neighbours[3];

	//uint16_t voxel_data; // if SPARSE_VOXELS: non-null block id   if !SPARSE_VOXELS: id to dense_chunks
	//uint16_t _pad;

	uint32_t _voxel_data;

	//uint16_t opaque_mesh_slices;
	//uint16_t transp_mesh_slices;
	uint32_t _transp_mesh_slices;

	uint32_t opaque_mesh_vertex_count;
	uint32_t transp_mesh_vertex_count;
};
bool chunk_sparse (in Chunk c) {
	uint32_t test = (c.flags & CHUNK_SPARSE_VOXELS);
	return test != 0;
}

ivec3 get_pos (in Chunk c) {
	return ivec3(c.posx, c.posy, c.posz);
}
uint32_t get_voxel_data (in Chunk c) {
	return c._voxel_data & 0xffffffffu;
}
uint32_t get_neighbour (in Chunk c, int i) {
	return c._neighbours[i >> 1] >> (i & 1) & 0xffffffffu;
}


struct ChunkVoxels {
	// data for all subchunks
	// sparse subchunk:  block_id of all subchunk voxels
	// dense  subchunk:  id of subchunk
	uint32_t sparse_data[SUBCHUNK_COUNT*SUBCHUNK_COUNT*SUBCHUNK_COUNT];

	// packed bits for all subchunks, where  0: dense subchunk  1: sparse subchunk
	uint64_t sparse_bits[SUBCHUNK_COUNT*SUBCHUNK_COUNT*SUBCHUNK_COUNT / 64];
};
uint32_t get_subchunk_idx (ivec3 pos) {
	pos >>= SUBCHUNK_SHIFT;
	uint32_t idx = pos.z << SUBCHUNK_SHIFT*2;
	idx += pos.y << SUBCHUNK_SHIFT;
	idx += pos.x;
	return idx;
}
bool is_subchunk_sparse (in ChunkVoxels dc, uint32_t subc_i) {
	uint64_t test = dc.sparse_bits[subc_i >> 6] & (1ul << (subc_i & 63));
	return test != 0;
}


struct SubchunkVoxels {
	uint32_t voxels[SUBCHUNK_SIZE][SUBCHUNK_SIZE][SUBCHUNK_SIZE/2];
};
uint32_t get_voxel (in SubchunkVoxels sc, ivec3 pos) {
	ivec3 masked = pos & ivec3(SUBCHUNK_MASK);
	return sc.voxels[masked.z][masked.y][masked.x >> 1] >> (masked.x&1) & 0xffffffffu;
}

layout(std430, binding = 1) readonly restrict buffer Chunks {
	Chunk chunks[];
};
layout(std430, binding = 2) readonly restrict buffer DenseChunks {
	ChunkVoxels dense_chunks[];
};
layout(std430, binding = 3) readonly restrict buffer DenseSubchunks {
	SubchunkVoxels dense_subchunks[];
};


layout(rgba16f, binding = 4) uniform image2D img;

uniform uint camera_chunk;

vec4 g_col = vec4(0,0,0,0);
bool _debug_col = false;

void DEBUG (vec4 col) {
	if (!_debug_col) {
		g_col = col;
		_debug_col = true;
	}
}

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

int normalize_float (float val) {
	if (val > 0) return +1;
	if (val < 0) return -1;
	return 0;
}

const float INF = 1. / 0.;

int find_next_axis (vec3 next) { // index of smallest component
	if (		next.x < next.y && next.x < next.z )	return 0;
	else if (	next.y < next.z )						return 1;
	else												return 2;
}

int get_step_face (int cur_axis, vec3 step_delta) {
	return cur_axis*2 +(step_delta[cur_axis] < 0 ? 1 : 0);
}

const float max_dist = 100.0;
const int max_iter = 100;

uint32_t chunk_id;

bool hit = false;
vec4 hit_col = vec4(0.8,0.8,0.8, 1.0);

bvec3 or (bvec3 l, bvec3 r) {
	return bvec3(ivec3(l) | ivec3(r));
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

bool hit_voxel (ivec3 pos, int entry_face, float dist) {
	if ( any(or( lessThan(pos, ivec3(0)), greaterThanEqual(pos, ivec3(CHUNK_SIZE)) )) ) {
		pos &= CHUNK_SIZE_MASK;
		return false;
	}
	
	if ((chunks[chunk_id].flags & CHUNK_SPARSE_VOXELS) != 0) return false;
	uint32_t dci = get_voxel_data(chunks[chunk_id]);
	
	//dense_chunks[dci]
	
	if (!(pos.x == pos.y || (pos.z == 0 && ((pos.x ^ pos.y) & 1) == 0)))
		return false; // no hit

	vec3 hit_pos_world = ray_pos + ray_dir * dist;
	
	vec2 uv = calc_uv(fract(hit_pos_world), entry_face);

	hit_col = ((pos.x ^ pos.y ^ pos.z) & 1) == 0 ? vec4(0.9, 0.4, 0.4, 1.0) : vec4(0.5, 0.5, 1.0, 1.0);
	hit_col.xy *= mix(vec2(0.2), vec2(1.0), uv);

	return true;
}

void trace_pixel (vec2 px_pos) {
	get_ray(px_pos);

	chunk_id = camera_chunk;
	vec3 chunk_pos = vec3(get_pos(chunks[chunk_id]) * float(CHUNK_SIZE));
	ray_pos -= chunk_pos;

	// voxel ray stepping setup
	ivec3 step_delta;
	step_delta.x = normalize_float(ray_dir.x);
	step_delta.y = normalize_float(ray_dir.y);
	step_delta.z = normalize_float(ray_dir.z);

	vec3 step_dist;
	step_dist.x = length(ray_dir / abs(ray_dir.x));
	step_dist.y = length(ray_dir / abs(ray_dir.y));
	step_dist.z = length(ray_dir / abs(ray_dir.z));

	vec3 pos_in_block = fract(ray_pos);

	ivec3 cur_voxel = ivec3(floor(ray_pos));

	vec3 next = step_dist * mix( 1.0 - pos_in_block, pos_in_block, step(ray_dir, vec3(0.0)) );

	// NaN -> Inf
	next = mix(next, vec3(INF), equal(ray_dir, vec3(0.0)));

	// find the axis of the next voxel step
	int   cur_axis = find_next_axis(next);
	float cur_dist = next[cur_axis];

	//DEBUG(vec4(ray_dir, 1.0));

	int iter = 0;
	while (!hit_voxel(cur_voxel, get_step_face(cur_axis, step_delta), cur_dist)) {

		// find the axis of the cur step
		cur_axis = find_next_axis(next);
		cur_dist = next[cur_axis];

		if (iter++ > max_iter) //  || cur_dist > max_dist
			return; // stop stepping because max_dist is reached

		// clac the distance at which the next voxel step for this axis happens
		next[cur_axis] += step_dist[cur_axis];
		// step into the next voxel
		cur_voxel[cur_axis] += step_delta[cur_axis];
	}

	hit = true;
}

void main () {
	vec2 pos = gl_GlobalInvocationID.xy;

	// maybe try not to do rays that we do not see (happens due to local group size)
	if (pos.x >= view.viewport_size.x || pos.y >= view.viewport_size.y)
		return;

	trace_pixel(pos);
	//col.rg = pos / view.viewport_size;

	if (_debug_col)
		hit_col = g_col;
	
	if (pos.x > 200 && hit)
		imageStore(img, ivec2(pos), hit_col);
}
