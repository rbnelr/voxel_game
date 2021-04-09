//// Utility

#ifdef _VERTEX
#define vs2fs out
#endif
#ifdef _FRAGMENT
#define vs2fs in
#endif

float map (float x, float a, float b) {
	return (x - a) / (b - a);
}

//// View

struct View {
	// forward
	mat4 world_to_clip;
	mat4 world_to_cam;
	mat4 cam_to_clip;
	// inverse
	mat4 clip_to_world;
	mat4 clip_to_cam;
	mat4 cam_to_world;

	// near clip plane distance (positive)
	float clip_near;
	// far clip plane distance (positive)
	float clip_far;
	// viewport size in pixels
	vec2 viewport_size;
};

//// Debug Line Rendering

struct glDrawArraysIndirectCommand {
	uint count;
	uint instanceCount;
	uint first;
	uint baseInstance;
};
struct IndirectLineDrawerVertex {
	vec4 pos;
	vec4 col;
};

layout(std430, binding = 1) restrict buffer IndirectLineDrawer {
	glDrawArraysIndirectCommand  cmd;
	IndirectLineDrawerVertex     vertices[];
} _line_drawer;
uniform uint _line_drawer_max_vertices = 1024;

void line_drawer_init () {
	_line_drawer.cmd.count         = 0;
	_line_drawer.cmd.instanceCount = 1;
	_line_drawer.cmd.first         = 0;
	_line_drawer.cmd.baseInstance  = 0;
}
void dbg_draw_vector (vec3 pos, vec3 dir, vec4 col) {
	if (_line_drawer.cmd.count < _line_drawer_max_vertices) {
		uint idx = _line_drawer.cmd.count;
		
		_line_drawer.vertices[idx].pos = vec4(pos, 0);
		_line_drawer.vertices[idx].col = col;
		idx++;
		
		_line_drawer.vertices[idx].pos = vec4(pos + dir, 0);
		_line_drawer.vertices[idx].col = col;
		idx++;
		
		_line_drawer.cmd.count = idx;
		//_line_drawer.cmd.count = idx % _line_drawer_max_vertices;
	}
}

//// Voxel world

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

#define SUBC_SPARSE_BIT 0x80000000u

#define B_AIR 1
#define B_WATER 3
#define B_MAGMA 12
#define B_CRYSTAL 15
#define B_URANIUM 16
#define B_LEAVES 18
#define B_TORCH 19
#define B_TALLGRASS 20

float get_emmisive (uint bid) {
	if (      bid == B_MAGMA   ) return 7.0;
	else if ( bid == B_CRYSTAL ) return 10.0;
	else if ( bid == B_URANIUM ) return 3.2;
	return 0.0;
}

#define FIXEDPOINT_FAC (1.0 / 256.0)

#define MERGE_INSTANCE_FACTOR 6
struct BlockMeshVertex {
	vec4 pos;
	vec4 normal;
	vec4 tangent;
	vec4 uv;
};

struct BlockTile {
	int sides[6];
	
	int anim_frames;
	int variants;
};


////
layout(std140, binding = 0) uniform Common {
	View view;
};

layout(std430, binding = 2) restrict readonly buffer BlockMeshes {
	BlockMeshVertex vertices[][MERGE_INSTANCE_FACTOR];
} block_meshes;

layout(std430, binding = 3) restrict readonly buffer BlockTiles {
	BlockTile block_tiles[];
};

uniform sampler2DArray	tile_textures;

	
	

