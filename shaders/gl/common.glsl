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

uniform vec3 fog_col = vec3(0.2, 0.27, 0.19);
uniform float fog_dens = 0.01;
uniform vec3 water_fog_col = vec3(0.02, 0.07, 0.1);
uniform float water_fog_dens = 0.06;
uniform float water_z = 0;

vec3 calc_fog (vec3 pix_col, vec3 pos_cam, vec3 pos_world) {
	// Incorrect HACK:
	// Would have to compute ray from cam to water surface, then from surface to underwater ground and do fog computation split
	// Simple volumetric raymarch would solve this and be really cool for a voxel game!
	float water_t = clamp(map(pos_world.z, water_z+1.0, water_z), 0.0, 1.0);
	
	vec3 f_col = mix(fog_col, water_fog_col, vec3(water_t));
	float dens = mix(fog_dens, water_fog_dens, water_t);
	
	float dist = length(pos_cam);
	float x = min(dist, 1000.0) * dens;
	float f = pow(2.0, -(x*x));
	
	return mix(f_col, pix_col, vec3(f));
}
