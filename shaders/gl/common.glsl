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

// theta is angle from 0,0,1
// phi is horizontal angle starting at 0,1,0
vec3 from_spherical (float theta, float phi, float r) {
	float st = sin(theta);
    float ct = cos(theta);
    return r * vec3(st * sin(phi), st * cos(phi), ct);
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

struct glDrawElementsIndirectCommand {
	uint count;
	uint primCount;
	uint firstIndex;
	uint baseVertex;
	uint baseInstance;
};

struct IndirectLineVertex {
	vec4 pos;
	vec4 col;
};
struct IndirectWireCubeInstace {
	vec4 pos;
	vec4 size;
	vec4 col;
};

struct IndirectLines {
	glDrawArraysIndirectCommand cmd;
	IndirectLineVertex vertices[4096];
};
struct IndirectWireCubes {
	glDrawElementsIndirectCommand cmd;
	uint _pad[3]; // padding for std430 alignment
	IndirectWireCubeInstace vertices[4096];
};

layout(std430, binding = 1) restrict buffer IndirectBuffer {
	IndirectLines     lines;
	IndirectWireCubes wire_cubes;
} _dbgdraw;

//void dbgdraw_clear () {
//	_dbgdraw.lines     .cmd.count = 0;
//	_dbgdraw.wire_cubes.cmd.primCount = 0;
//}
void dbgdraw_vector (vec3 pos, vec3 dir, vec4 col) {
	uint idx = atomicAdd(_dbgdraw.lines.cmd.count, 2u);
	if (idx < 4096) {
		_dbgdraw.lines.vertices[idx++] = IndirectLineVertex( vec4(pos      , 0), col );
		_dbgdraw.lines.vertices[idx  ] = IndirectLineVertex( vec4(pos + dir, 0), col );
		//_dbgdraw.lines.cmd.count %= 4096;
	}
}
void dbgdraw_wire_cube (vec3 pos, vec3 size, vec4 col) {
	uint idx = atomicAdd(_dbgdraw.wire_cubes.cmd.primCount, 1u);
	if (idx < 4096) {
		_dbgdraw.wire_cubes.vertices[idx] =
			IndirectWireCubeInstace( vec4(pos, 0), vec4(size, 0), col);
		//_dbgdraw.wire_cubes.cmd.primCount %= 4096;
	}
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

uniform sampler2DArray	textures_A;
uniform sampler2DArray	textures_N;
uniform sampler2DArray	textures2_A;
uniform sampler2DArray	textures2_N;

