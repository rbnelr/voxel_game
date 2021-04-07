#version 460 core
#if VISUALIZE_COST && VISUALIZE_WARP_COST
	#extension GL_KHR_shader_subgroup_arithmetic : enable
	#extension GL_ARB_shader_group_vote : enable
#endif

#define LOCAL_SIZE_X WORKGROUP_SIZE
#define LOCAL_SIZE_Y 1

layout(local_size_x = LOCAL_SIZE_X) in;

#define ONLY_PRIMARY_RAYS 0

#include "rt_util.glsl"

#define FIXEDPOINT_FAC (1.0 / 256.0)
struct BlockMeshInstance {
	//int16_t		posx, posy, posz; // pos in chunk
	//uint16_t	meshid; // index for merge instancing, this is used to index block meshes
	//uint16_t	texid; // texture array id based on block id
	//uint16_t	_pad; // padding to allow reading as buffer in compute shader which does not support 16 bit ints
	
	uint posxy;
	uint posz_meshid;
	uint texid_pad;
};

layout(std430, binding = 3) readonly buffer SliceInstaces {
	BlockMeshInstance instances[];
} faces;

layout(std430, binding = 4) writeonly buffer SliceLighting {
	vec4 instances[];
} lighting;

uniform vec3 chunk_pos;

uniform uint vertex_count;

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
	
	srand(idx);
	
	vec3 pos;
	{
		int xy = int(faces.instances[idx].posxy);
		int z = int(faces.instances[idx].posz_meshid);
		
		 // xy & 0xffff will not sign extent correctly, while `int >>` will
		ivec3 ipos = ivec3(xy << 16, xy, z << 16) >> 16;
		
		pos = vec3(ipos) * FIXEDPOINT_FAC;
	}
	
	//lighting.instances[idx] = vec4(float(idx) / 6000.0);
	
	//lighting.instances[idx] = vec4(pos.xyz / 64.0, 1.0);
	lighting.instances[idx] = vec4(vec3(pos.x / 64.0), 1.0);
}
