#version 460 core
#extension GL_NV_gpu_shader5: enable // for uint8_t
#extension GL_NV_shader_thread_shuffle: enable

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(r8ui, binding = 4) restrict uniform uimage3D df_img;

#include "gpu_voxels.glsl"

const int REGION = 8;
const int CORE = REGION -2;
const int CHUNK_WGROUPS = (CHUNK_SIZE + CORE-1) / CORE; // round up

shared uint8_t buf[REGION][REGION][REGION];

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
	uint8_t val = bid > B_AIR ? uint8_t(1u) : uint8_t(0u);
	
#if 0
	buf[z][y][x] = val;
	barrier(); // make write visible to X pass reads
	
	// X pass
	if (x >         0) val |= buf[z][y][x-1];
	if (x < REGION -1) val |= buf[z][y][x+1];
	
    barrier(); // finish reads so we can write X pass result
#else
	// X pass (with lane intrinsics)
	// NOTE: if (x > 0) range checks are not needed since out of bounds reads return this threads value, which can safely be ORed in
	uint8_t val0 = uint8_t(shuffleDownNV(val, 1u, REGION));
	uint8_t val1 = uint8_t(shuffleUpNV  (val, 1u, REGION));
	val |= val0;
	val |= val1;
#endif
	buf[z][y][x] = val;
	barrier(); // make write visible to Y pass reads
	
	{ // Y pass
		if (y >         0) val |= buf[z][y-1][x];
		if (y < REGION -1) val |= buf[z][y+1][x];
	}
    barrier(); // finish reads so we can write Y pass result
	buf[z][y][x] = val;
	
	if ( x > 0 && x < REGION-1 &&
		 y > 0 && y < REGION-1 &&
		 z > 0 && z < REGION-1 && in_chunk ) {
		barrier(); // make write visible to Z pass reads
		
		// Z pass
		val |= buf[z-1][y][x];
		val |= buf[z+1][y][x];
		
		imageStore(df_img, pos, uvec4(val != 0u ? 0u : 255u, 0u,0u,0u));
	}
}
