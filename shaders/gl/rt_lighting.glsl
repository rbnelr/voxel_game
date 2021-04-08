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

#include "rt_util.glsl"

struct BlockMeshInstance {
	int16_t		posx, posy, posz; // pos in chunk
	uint16_t	meshid; // index for merge instancing, this is used to index block meshes
	uint16_t	texid; // texture array id based on block id
};

layout(std430, binding = 3) readonly buffer SliceInstaces {
	BlockMeshInstance instances[];
} faces;

layout(std430, binding = 4) writeonly buffer SliceLighting {
	vec4 instances[];
} lighting;

uniform vec3 chunk_pos;

uniform uint vertex_count;

// get face from slice instances, return position of (negative axis corner) quad
// and matrix to offset positions on quad or to rotate direction vectors
void get_face (uint idx, out vec3 pos, out mat3 TBN) {
	BlockMeshInstance inst = faces.instances[idx];
	
	pos = vec3(inst.posx, inst.posy, inst.posz) * FIXEDPOINT_FAC + chunk_pos;
	uint meshid = faces.instances[idx].meshid;
	
	pos = round(pos); // round jittered coords to get back voxel coord
	
	// seed rng with world integer coord of voxel
	srand(int(pos.x) * 12 + meshid, int(pos.y), int(pos.z));
	
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
	pos += ((a+b)+(c+d)+(e+f)) * 0.1666667;
}

uniform int samples = 16;

void main () {
#if VISUALIZE_COST && VISUALIZE_WARP_COST
	if (subgroupElect()) {
		warp_iter[gl_SubgroupID] = 0u;
	}

	barrier();
#endif
	
	uint idx = gl_GlobalInvocationID.x;
	if (idx >= vertex_count)
		return;
	
	vec3 pos;
	mat3 TBN;
	get_face(idx, pos, TBN);
	
	//lighting.instances[idx] = vec4(float(idx) / 6000.0);
	//lighting.instances[idx] = vec4(vec3((pos.x - 64.0) / 64.0), 1.0);
	
	vec3 col = vec3(0.0);
	
	for (int i=0; i<samples; ++i) {
		vec2 offs = rand2() - 0.5;
		vec3 ray_pos = TBN * vec3(offs, 0.005) + pos;
		
		iterations = 0;
		vec3 light = collect_sunlight(ray_pos, TBN[2]);
		
		if (bounces_enable) {
			
			float max_dist = bounces_max_dist;
			
			vec3 cur_normal = TBN[2];
			vec3 contrib = vec3(1.0);
			
			for (int j=0; j<bounces_max_count; ++j) {
				vec3 dir = get_tangent_to_world(cur_normal) * hemisphere_sample(); // already cos weighted
				
				Hit hit2;
				if (!trace_ray_refl_refr(ray_pos, dir, max_dist, hit2))
					break;
				
				vec3 light2 = collect_sunlight(ray_pos, cur_normal);
				
				light += (hit2.emiss + hit2.col * light2) * contrib;
				
				ray_pos = hit2.pos + hit2.normal * 0.001;
				max_dist -= hit2.dist;
				
				cur_normal = hit2.normal;
				contrib *= hit2.col;
			}
		}
		
		col += light;
	}
	
	col /= float(samples);
	
	lighting.instances[idx] = vec4(col, 1.0);
}
