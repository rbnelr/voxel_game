#version 460 core
layout(local_size_x = GROUPSZ, local_size_y = GROUPSZ) in;

#include "gpu_voxels.glsl"

layout(r8ui, binding = 4) restrict uniform uimage3D df_img;

#define SIZE CHUNK_SIZE

uniform ivec3 offsets[64];

void main () {
#if PASS == 0
	ivec3 pos = ivec3(0, gl_GlobalInvocationID.xy);
	pos += offsets[gl_WorkGroupID.z];
	
	int prev = 255;
	for (int x=0; x<SIZE; ++x) {
		uint bid = texelFetch(voxel_tex, pos + ivec3(x,0,0), 0).r;
		
		int val = bid > B_AIR ? 0 : prev+1;
		
		imageStore(df_img, pos + ivec3(x,0,0), uvec4(uint(val), 0u,0u,0u));
		prev = val;
	}
	
	prev = 255;
	for (int x=SIZE-1; x>=0; --x) {
		int cur = int(imageLoad(df_img, pos + ivec3(x,0,0)).r);
		
		int val = min(cur, prev+1);
		//if (val != cur) // TODO: avoiding writes better for performance??
		imageStore(df_img, pos + ivec3(x,0,0), uvec4(uint(val), 0u,0u,0u));
		prev = val;
	}
	
#elif PASS == 1
	ivec3 pos = ivec3(gl_GlobalInvocationID.x, 0, gl_GlobalInvocationID.y);
	pos += offsets[gl_WorkGroupID.z];
	
	int prev = 255;
	for (int y=0; y<SIZE; ++y) {
		int cur = int(imageLoad(df_img, pos + ivec3(0,y,0)).r);
		
		int val = min(cur, prev+1);
		//if (val != cur) // TODO: avoiding writes better for performance??
		imageStore(df_img, pos + ivec3(0,y,0), uvec4(uint(val), 0u,0u,0u));
		prev = val;
	}
	
	prev = 255;
	for (int y=SIZE-1; y>=0; --y) {
		int cur = int(imageLoad(df_img, pos + ivec3(0,y,0)).r);
		
		int val = min(cur, prev+1);
		//if (val != cur) // TODO: avoiding writes better for performance??
		imageStore(df_img, pos + ivec3(0,y,0), uvec4(uint(val), 0u,0u,0u));
		prev = val;
	}
	
#else
	ivec3 pos = ivec3(gl_GlobalInvocationID.xy, 0);
	pos += offsets[gl_WorkGroupID.z];
	
	int prev = 255;
	for (int z=0; z<SIZE; ++z) {
		int cur = int(imageLoad(df_img, pos + ivec3(0,0,z)).r);
		
		int val = min(cur, prev+1);
		//if (val != cur) // TODO: avoiding writes better for performance??
		imageStore(df_img, pos + ivec3(0,0,z), uvec4(uint(val), 0u,0u,0u));
		prev = val;
	}
	
	prev = 255;
	for (int z=SIZE-1; z>=0; --z) {
		int cur = int(imageLoad(df_img, pos + ivec3(0,0,z)).r);
		
		int val = min(cur, prev+1);
		//if (val != cur) // TODO: avoiding writes better for performance??
		imageStore(df_img, pos + ivec3(0,0,z), uvec4(uint(val), 0u,0u,0u));
		prev = val;
	}
#endif
}
