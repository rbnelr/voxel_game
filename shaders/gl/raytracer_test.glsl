#version 460 core // for GL_ARB_shader_draw_parameters
#extension GL_NV_gpu_shader5 : enable

#include "common.glsl"

layout(local_size_x = 1, local_size_y = 1) in;

#define CHUNK_SIZE			64 // size of chunk in blocks per axis
#define CHUNK_SIZE_SHIFT	6 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		63

#define SUBCHUNK_SIZE		4 // size of subchunk in blocks per axis
#define SUBCHUNK_COUNT		16 // size of chunk in subchunks per axis
#define SUBCHUNK_SHIFT		2
#define SUBCHUNK_MASK		3


struct SubchunkVoxels {
	uint16_t voxels[SUBCHUNK_SIZE][SUBCHUNK_SIZE][SUBCHUNK_SIZE];
};

layout(std430, binding = 1) buffer DenseSubchunks {
	SubchunkVoxels dense_subchunks[];
};


struct ChunkVoxels {
	// data for all subchunks
	// sparse subchunk:  block_id of all subchunk voxels
	// dense  subchunk:  id of subchunk
	uint32_t sparse_data[SUBCHUNK_COUNT*SUBCHUNK_COUNT*SUBCHUNK_COUNT];

	// packed bits for all subchunks, where  0: dense subchunk  1: sparse subchunk
	uint64_t sparse_bits[SUBCHUNK_COUNT*SUBCHUNK_COUNT*SUBCHUNK_COUNT / 64];
};
bool is_subchunk_sparse (in ChunkVoxels dc, uint32_t subc_i) {
	uint64_t test = dc.sparse_bits[subc_i >> 6] & (1ul << (subc_i & 63));
	return test != 0;
}

layout(std430, binding = 2) buffer DenseChunks {
	ChunkVoxels dense_chunks[];
};

#define MAX_CHUNK_SLICES	32

#define CHUNK_SPARSE_VOXELS	 4

struct ChunkMesh {
	uint32_t vertex_count;
	uint16_t slices[MAX_CHUNK_SLICES];
};
struct Chunk {
	uint32_t flags;
	ivec3 pos;

	uint8_t loadphase;
	uint16_t neighbours[6];

	uint16_t voxel_data; // if SPARSE_VOXELS: non-null block id   if !SPARSE_VOXELS: id to dense_chunks

	ChunkMesh opaque_mesh;
	ChunkMesh transparent_mesh;
};
bool chunk_sparse (in Chunk c) {
	uint32_t test = (c.flags & CHUNK_SPARSE_VOXELS);
	return test != 0;
}

layout(std430, binding = 3) buffer Chunks {
	Chunk chunks[];
};


layout(rgba16f, binding = 4) uniform image2D img;

vec4 g_col = vec4(0,0,0,0);
bool _debug_col = false;

void DEBUG (vec4 col) {
	if (!_debug_col) {
		g_col = col;
		_debug_col = true;
	}
}

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

	DEBUG(vec4(ray_dir, 1));
	//ray_pos -= vec3(svo_root_pos);
}

vec4 trace_pixel (vec2 px_pos) {
	vec3 ray_pos, ray_dir;
	get_ray(px_pos, ray_pos, ray_dir);

	return vec4(ray_dir, 1.0);
}

void main () {
	vec2 pos = gl_GlobalInvocationID.xy;

	vec4 col = trace_pixel(pos);
	//col.rg = pos / view.viewport_size;

	if (_debug_col)
		col = g_col;
	imageStore(img, ivec2(pos), col);
}
