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
		vec3 tangent = abs(normal.x) >= 0.9 ? vec3(0,1,0) : vec3(1,0,0);
		vec3 bitangent = cross(normal, tangent);
		tangent = cross(bitangent, normal);
		
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
#define B_TORCH 19
#define B_TALLGRASS 20

float get_alpha_mult (uint bid) {
	if (      bid == B_WATER   ) return 1.00;
	else if ( bid == B_CRYSTAL ) return 0.90;
	return 1.0;
}
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
uint prev_chunk_id;

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

struct Hit {
	vec3	pos;
	vec3	normal;
	float	dist;
	
	vec4	col;
	vec3	emiss;
};

bool hit_voxel (uint bid, vec3 proj, float dist, int axis, vec3 ray_dir, float step_size, inout Hit hit) {
	if (bid == B_AIR)
		return false;

	hit.pos = proj;
	
	int entry_face = get_step_face(axis, ray_dir);
	
	vec2 uv = calc_uv(fract(proj), entry_face);

	vec3 normal = vec3(0.0);
	normal[axis] = -sign(ray_dir[axis]);
	
	hit.normal = normal;

	float texid = float(block_tiles[bid].sides[entry_face]);
	
	vec4 col = texture(tile_textures, vec3(uv, texid));
	
	if (bid == B_TALLGRASS && axis == 2)
		col = vec4(0.0);
	
	col.a *= get_alpha_mult(bid);
	
	vec3 emiss = col.rgb * get_emmisive_mult(bid);
	col.rgb += emiss;
	hit.emiss += emiss; // accumulate emitted light
	
	col.a *= step_size; // try to account for large steps through subchunks, what's the real formula for this?
	
	col.a = min(col.a, 1.0);
	
	// accum color
	col.a *= 1.0 - hit.col.a;
	hit.emiss *= 1.0 - hit.col.a;
	
	col.rgb *= col.a;
	hit.col += col;
	// accum 
	
	hit.dist = dist;
	return hit.col.a > 0.99;
}

bool trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, inout Hit hit) {
	ivec3 coord = ivec3(floor(ray_pos));

	vec3 rdir; // reciprocal of ray dir
	rdir.x = ray_dir.x != 0.0 ? 1.0 / abs(ray_dir.x) : INF;
	rdir.y = ray_dir.y != 0.0 ? 1.0 / abs(ray_dir.y) : INF;
	rdir.z = ray_dir.z != 0.0 ? 1.0 / abs(ray_dir.z) : INF;

	int axis;
	vec3 proj;
	{
		vec3 rel = ray_pos - vec3(coord);
		
		vec3 plane_offs;
		plane_offs.x = ray_dir.x >= 0.0 ? 1.0 - rel.x : rel.x;
		plane_offs.y = ray_dir.y >= 0.0 ? 1.0 - rel.y : rel.y;
		plane_offs.z = ray_dir.z >= 0.0 ? 1.0 - rel.z : rel.z;
		
		vec3 next = rdir * plane_offs;
		axis = find_next_axis(next);
		
		proj = ray_pos + ray_dir * next[axis];
		
	}

	int step_mask = -1;
	float dist = 0.0;
	
	float step_size;
	uint prev_bid = read_voxel(step_mask, coord, step_size);
	
	while (iterations < max_iterations) {
		iterations++;

		uint bid = read_voxel(step_mask, coord, step_size);

		if (prev_bid != bid && hit_voxel(bid, proj, dist, axis, ray_dir, step_size, hit))
			return true;
		prev_bid = bid;

		vec3 rel = ray_pos - vec3(coord);
		
		vec3 plane_offs;
		plane_offs.x = ray_dir.x >= 0.0 ? step_size - rel.x : rel.x;
		plane_offs.y = ray_dir.y >= 0.0 ? step_size - rel.y : rel.y;
		plane_offs.z = ray_dir.z >= 0.0 ? step_size - rel.z : rel.z;

		vec3 next = rdir * plane_offs;
		axis = find_next_axis(next);
		
		float min_step_dist = 0.0001;
		proj = ray_pos + ray_dir * (dist + max(next[axis] - dist, min_step_dist));
		
		dist = next[axis];

		if (next[axis] > max_dist)
			break;

		ivec3 old_coord = coord;
		
		vec3 tmp = proj;
		tmp[axis] += ray_dir[axis] >= 0 ? 0.5 : -0.5;
		
		coord = ivec3(floor(tmp));
		
		ivec3 step_mask3 = coord ^ old_coord;
		step_mask = step_mask3.x | step_mask3.y | step_mask3.z;
		
		prev_chunk_id = chunk_id;

		// handle step out of chunk by checking bits
		if ((step_mask & ~CHUNK_SIZE_MASK) != 0) {
			chunk_id = get_neighbour(chunk_id, get_step_face(axis, ray_dir) ^ 1); // ^1 flip dir
			if (chunk_id == 0xffffu)
				break;
		}
	}

	return false;
}

vec3 tonemap (vec3 c) {
	
	//c *= 2.0;
	//c = c / (c + 1.0);
	
	//c *= 0.2;
	//vec3 x = max(vec3(0.0), c -0.004);
	//c = (x*(6.2*x+.5))/(x*(6.2*x+1.7)+0.06);
	return c;
}


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
	Hit hit;
	hit.col = vec4(0.0);
	hit.emiss = vec3(0.0);
	if (trace_ray(ray_pos, ray_dir, INF, hit)) {
		
		uint start_chunk = prev_chunk_id;
		vec3 bounce_pos = hit.pos + hit.normal * 0.001;
		
		// secondary rays
		
		vec3 light = vec3(0.00);
		
		if (sunlight_enable) {
			// fake directional light
			float accum = 0.0;
			for (int i=0; i<sunlight_rays; ++i) {
				chunk_id = start_chunk;
		
				ray_dir = normalize(vec3(1,2,3)) + rand3()*0.05;
		
				Hit hit2;
				hit2.col = vec4(0.0);
				hit2.emiss = vec3(0.0);
				if (trace_ray(bounce_pos, ray_dir, sunlight_dist, hit2))
					accum += hit2.col.a;
			}
		
			light += (1.0 - accum / float(sunlight_rays)) * sunlight_col;
		}
		
		float ambient = 0.5;
		
		if (ambient_enable) {
			
			// hemisphere AO
			float accum = 0.0;
			
			for (int i=0; i<ambient_rays; ++i) {
				chunk_id = start_chunk;
				
				Hit hit2;
				hit2.col = vec4(0.0);
				hit2.emiss = vec3(0.0);
				if (trace_ray(bounce_pos, get_bounce_dir(hit.normal), ambient_dist, hit2)) {
					accum += clamp(hit2.col.a, 0.0, 1.0);
				}
			}
			
			ambient = pow(1.0 - accum / float(ambient_rays), 1.5);
		}
		
		light += ambient_col * ambient;
		
		if (bouncelight_enable) {
			// Lighting in 64 block radius from emissive blocks
			vec3 accum = vec3(0.0);
			for (int i=0; i<bouncelight_rays; ++i) {
				chunk_id = start_chunk;
				
				Hit hit2;
				hit2.col = vec4(0.0);
				hit2.emiss = vec3(0.0);
				trace_ray(bounce_pos, get_bounce_dir(hit.normal), bouncelight_dist, hit2);
				
				accum += hit2.emiss;
			}
			
			light += accum / float(bouncelight_rays);
		}
		
		hit.col.rgb *= light;
		
		if (visualize_light)
			hit.col.rgb = vec3(light);
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
