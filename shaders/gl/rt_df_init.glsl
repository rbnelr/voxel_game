#version 460 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(r8ui, binding = 4) restrict uniform uimage3D df_img;

#include "gpu_voxels.glsl"

const int REGION = 8;
const int CORE = REGION -2;
const int CHUNK_WGROUPS = (CHUNK_SIZE + CORE-1) / CORE; // round up

shared int buf[REGION][REGION][REGION];

uniform ivec3 offsets[32];

void main () {
	ivec3 wgroupid = ivec3(gl_WorkGroupID);
	
	int chunkid = wgroupid.z / CHUNK_WGROUPS;
	wgroupid.z  = wgroupid.z % CHUNK_WGROUPS;
	
	ivec3 pos = wgroupid * CORE + ivec3(gl_LocalInvocationID)-1; // pos in chunk
	bool in_chunk = all(lessThan(pos, ivec3(CHUNK_SIZE)));
	
	pos += offsets[chunkid]; // to world coord
	
	int x = int(gl_LocalInvocationID.x);
	int y = int(gl_LocalInvocationID.y);
	int z = int(gl_LocalInvocationID.z);
	
	uint bid = in_chunk ? texelFetch(voxel_tex, pos, 0).r : 0;
	int val = bid > B_AIR ? 1 : 0;
	
	buf[z][y][x] = val;
	barrier();
	
	// X pass
	if (x >         0) val |= buf[z][y][x-1];
	if (x < REGION -1) val |= buf[z][y][x+1];
	
    barrier();
	buf[z][y][x] = val;
	barrier();
	
	// Y pass
	if (y >         0) val |= buf[z][y-1][x];
	if (y < REGION -1) val |= buf[z][y+1][x];
	
    barrier();
	buf[z][y][x] = val;
	barrier();
	
	if ( x > 0 && x < REGION-1 &&
		 y > 0 && y < REGION-1 &&
		 z > 0 && z < REGION-1 && in_chunk ) {
		
		// Z pass
		val |= buf[z-1][y][x];
		val |= buf[z+1][y][x];
		
		imageStore(df_img, pos, uvec4(val != 0 ? 0u : 255u, 0u,0u,0u));
	}
}
