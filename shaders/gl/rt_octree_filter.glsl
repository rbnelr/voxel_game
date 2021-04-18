#version 460 core
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(r8ui, binding = 4) writeonly restrict uniform uimage3D write_mip;

#include "gpu_voxels.glsl"

uniform uvec3 offsets[16];
uniform uint size;

#if MIP0
	void main () {
		uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);
		
		uint chunk_idx = pos.z / size;
		pos.z          = pos.z % size;
		
		uvec3 offset = offsets[chunk_idx];
		pos += offset;
		
		uint bid = read_bid(pos);
		imageStore(write_mip, ivec3(pos), uvec4(bid, 0u,0u,0u));
	}
#else
	uniform int read_mip;

	void main () {
		uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);

		uint chunk_idx = pos.z / size;
		pos.z          = pos.z % size;

		if (all(lessThan(pos, uvec3(size)))) {
			uvec3 offset = offsets[chunk_idx];
			pos += offset;

			uvec3 read_pos = pos * 2u;
	
			uint a = texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(0,0,0)).r;
			uint b = texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(1,0,0)).r;
			uint c = texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(0,1,0)).r;
			uint d = texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(1,1,0)).r;
			uint e = texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(0,0,1)).r;
			uint f = texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(1,0,1)).r;
			uint g = texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(0,1,1)).r;
			uint h = texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(1,1,1)).r;
			
			bool same = a == b;
			same      = a == c && same;
			same      = a == d && same;
			same      = a == e && same;
			same      = a == f && same;
			same      = a == g && same;
			same      = a == h && same;
			
			imageStore(write_mip, ivec3(pos), uvec4(same ? a : 0xffu, 0u,0u,0u));
		}
	}
#endif
