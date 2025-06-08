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

const float INF			= 99999999999999999999999999.0; // 1. / 0. breaks on AMD

const float PI			= 3.1415926535897932384626433832795;

const float DEG2RAD		= 0.01745329251994329576923690768489;
const float RAD2DEG		= 57.295779513082320876798154814105;

const float SQRT_2	    = 1.4142135623730950488016887242097;
const float SQRT_3	    = 1.7320508075688772935274463415059;

const float HALF_SQRT_2	= 0.70710678118654752440084436210485;
const float HALF_SQRT_3	= 0.86602540378443864676372317075294;

const float INV_SQRT_2	= 0.70710678118654752440084436210485;
const float INV_SQRT_3	= 0.5773502691896257645091487805019;

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
	
	vec2 _pad;
	
	// viewport size in pixels
	vec2 viewport_size;
	// 1 / viewport_size * 2
	vec2 inv_viewport_size2;
	
	// for simpler RT get_ray calculation
	vec3 frust_x; // from center of near plane to right edge
	vec3 frust_y; // from center of near plane to top edge
	vec3 frust_z; // from camera center to center of near plane
	
	vec3 cam_pos; // cam position
	vec3 cam_forw; // cam position
	
	vec3 lod_center;
};

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

