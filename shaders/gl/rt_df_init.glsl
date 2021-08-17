#version 460 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(r8ui, binding = 4) restrict uniform uimage3D df_img;

#include "gpu_voxels.glsl"

#define DISPATCH_SIZE uint((CHUNK_SIZE+13) / 14)

uniform ivec3 offsets[32];

shared uint bits[16][16];

void main () {
	uvec2 threadid = gl_LocalInvocationID.xy;
	
	uvec3 groupid = gl_WorkGroupID;
	groupid.z %= DISPATCH_SIZE;
	
	uint chunkid = gl_WorkGroupID.z / DISPATCH_SIZE;
	
	ivec3 pos = (ivec3(groupid) * 14 - 1) + ivec3(threadid,0);
	ivec3 world_pos = pos + offsets[chunkid];
	
	// Get solidness bits
	uint b = 0u;
	for (int z=0; z<16; ++z) {
		uint bid = texelFetch(voxel_tex, world_pos + ivec3(0,0,z), 0).r;
		uint val = bid > B_AIR ? 1u : 0u;
		
		b |= val << z;
	}
	
	// Z spread Pass
	b |= b<<1;
	b |= b>>1;
	
	bits[threadid.y][threadid.x] = b;
	
	barrier();
	
	// X spread Pass
	if (threadid.x < 15) b |= bits[threadid.y][threadid.x+1];
	if (threadid.x >  0) b |= bits[threadid.y][threadid.x-1];
	
	bits[threadid.y][threadid.x] = b;
	
	barrier();
	
	// Y spread Pass
	if (threadid.y < 15) b |= bits[threadid.y+1][threadid.x];
	if (threadid.y >  0) b |= bits[threadid.y-1][threadid.x];
	
	for (int z=1; z<15; ++z) {
		uvec3 pos_in_chunk = world_pos + ivec3(0,0,z) - offsets[chunkid];
		
		if (  all(lessThan(pos_in_chunk, uvec3(CHUNK_SIZE))) &&
			  all(lessThan(threadid-1u, uvec2(15)))) {
			uint val = ((b >> z) & 1u) == 1u ? 0u : 255u;
			imageStore(df_img, world_pos + ivec3(0,0,z), uvec4(val, 0u,0u,0u));
		}
	}
}
