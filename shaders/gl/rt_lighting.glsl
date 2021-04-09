#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t

#if VISUALIZE_COST && VISUALIZE_WARP_COST
	#extension GL_KHR_shader_subgroup_arithmetic : enable
	#extension GL_ARB_shader_group_vote : enable
#endif

#define LOCAL_SIZE_X WORKGROUP_SIZE
#define LOCAL_SIZE_Y 1

layout(local_size_x = LOCAL_SIZE_X) in;

#define ONLY_PRIMARY_RAYS 0

#define RT_LIGHT 1
#include "rt_util.glsl"

struct BlockMeshInstance {
	int16_t		posx, posy, posz; // pos in chunk
	uint16_t	meshid; // index for merge instancing, this is used to index block meshes
	uint16_t	texid; // texture array id based on block id
};

layout(std430, binding = 5) restrict readonly buffer SliceInstaces {
	BlockMeshInstance instances[];
} faces;

layout(std430, binding = 6) restrict buffer SliceLighting {
	vec4 instances[];
} lighting;

uniform vec3 chunk_pos;

uniform uint vertex_count;

uniform float taa_alpha = 0.05;
uniform uint rand_frame_index = 0;

uniform ivec3 _dbg_ray_pos;

// get face from slice instances, return position of (negative axis corner) quad
// and matrix to offset positions on quad or to rotate direction vectors
void get_face (uint idx, out vec3 pos, out vec3 face_center, out mat3 TBN) {
	BlockMeshInstance inst = faces.instances[idx];
	
	pos = vec3(inst.posx, inst.posy, inst.posz) * FIXEDPOINT_FAC + chunk_pos;
	uint meshid = faces.instances[idx].meshid;
	
	pos = round(pos); // round jittered coords to get back voxel coord
	
	// seed rng with world integer coord of voxel
	srand(
		int(pos.x) * 13 + meshid,
		int(pos.y) * 3 + rand_frame_index * 6151, // arbitary primes
		int(pos.z));
	
	vec3 normal = block_meshes.vertices[meshid][0].normal.xyz;
	vec3 tangent = block_meshes.vertices[meshid][0].tangent.xyz;
	
	// This is stupid, need a good way of getting a 
	vec3 a = block_meshes.vertices[meshid][0].pos.xyz;
	vec3 b = block_meshes.vertices[meshid][1].pos.xyz;
	vec3 c = block_meshes.vertices[meshid][2].pos.xyz;
	vec3 d = block_meshes.vertices[meshid][3].pos.xyz;
	vec3 e = block_meshes.vertices[meshid][4].pos.xyz;
	vec3 f = block_meshes.vertices[meshid][5].pos.xyz;
	
	vec3 bitangent = cross(normal, tangent);
	
	TBN = mat3(tangent, bitangent, normal);
	face_center = pos + ((a+b)+(c+d)+(e+f)) * 0.1666667;
}

uniform int samples = 16;

void main () {
	uint idx = gl_GlobalInvocationID.x;
	if (idx >= vertex_count)
		return;
	
	vec3 vox_pos;
	vec3 face_center;
	mat3 TBN;
	get_face(idx, vox_pos, face_center, TBN);
	
	_dbg_ray = update_debug_rays && all(equal(ivec3(vox_pos), _dbg_ray_pos));
	if (_dbg_ray) line_drawer_init();
	
	vec3 col = vec3(0.0);
	
	for (int i=0; i<samples; ++i) {
		vec2 offs = rand2() - 0.5;
		//vec2 offs = vec2(0);
		vec3 ray_pos = TBN * vec3(offs, 0.005) + face_center;
		
		iterations = 0;
		vec3 light = collect_sunlight(ray_pos, TBN[2]);
		
		if (bounces_enable) {
			
			float max_dist = bounces_max_dist;
			
			vec3 cur_normal = TBN[2];
			vec3 contrib = vec3(1.0);
			
			#if 1
			vec3 dir = get_tangent_to_world(cur_normal) * hemisphere_sample_stratified(i, samples);
			#else
			vec3 dir = get_tangent_to_world(cur_normal) * hemisphere_sample(); // already cos weighted
			#endif
			
			for (int j=0; j<bounces_max_count; ++j) {
				Hit hit2;
				if (!trace_ray_refl_refr(ray_pos, dir, max_dist, hit2))
					break;
				
				ray_pos = hit2.pos + hit2.normal * 0.001;
				max_dist -= hit2.dist;
				
				cur_normal = hit2.normal;
				
				dir = get_tangent_to_world(cur_normal) * hemisphere_sample(); // already cos weighted
				
				vec3 light2 = collect_sunlight(ray_pos, cur_normal);
				
				light += (hit2.emiss + hit2.col * light2) * contrib;
				contrib *= hit2.col;
			}
		}
		
		col += light;
	}
	
	col /= float(samples);
	
	vec3 prev = lighting.instances[idx].rgb;
	lighting.instances[idx] = vec4(mix(prev, col, taa_alpha), 1.0);
}
