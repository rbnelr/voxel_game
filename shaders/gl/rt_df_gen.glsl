#version 460 core
layout(local_size_x = GROUPSZ, local_size_y = GROUPSZ) in;

layout(r8ui, binding = 4) restrict uniform uimage3D df_img;

#include "gpu_voxels.glsl"
#define SIZE CHUNK_SIZE

uniform ivec3 offsets[32];

void main () {
#if PASS == 0
	ivec3 pos = ivec3(0, gl_GlobalInvocationID.xy);
	#define GETPOS(I) ivec3(pos.x + (I), pos.y, pos.z)
#elif PASS == 1
	ivec3 pos = ivec3(gl_GlobalInvocationID.x, 0, gl_GlobalInvocationID.y);
	#define GETPOS(I) ivec3(pos.x, pos.y + (I), pos.z)
#else
	ivec3 pos = ivec3(gl_GlobalInvocationID.xy, 0);
	#define GETPOS(I) ivec3(pos.x, pos.y, pos.z + (I))
#endif
	
	pos += offsets[gl_WorkGroupID.z];
	
	uint prev = 255u;
	for (int i=0; i<SIZE; ++i) {
		ivec3 p = GETPOS(i);
		uint cur = imageLoad(df_img, p).r;
		
		prev += 1u;
		if (prev < cur) imageStore(df_img, p, uvec4(prev, 0u,0u,0u));
		else            prev = cur;
	}
	
	prev = 255u;
	for (int i=SIZE-1; i>=0; --i) {
		ivec3 p = GETPOS(i);
		uint cur = imageLoad(df_img, p).r;
		
		prev += 1u;
		if (prev < cur) imageStore(df_img, p, uvec4(prev, 0u,0u,0u));
		else            prev = cur;
	}
}
