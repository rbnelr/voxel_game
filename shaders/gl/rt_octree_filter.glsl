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
		
		uvec3 read_pos = pos * 2u;
		
		uint value;
		
		{ // read_bid inlined because subchunks are all the same
			uvec3 texcoord = bitfieldExtract(read_pos, SUBCHUNK_SHIFT, 32 - SUBCHUNK_SHIFT); // (coord & ~SUBCHUNK_MASK) >> SUBCHUNK_SHIFT;
			uint subchunk = texelFetch(voxels[0], ivec3(texcoord), 0).r;
			
			if ((subchunk & SUBC_SPARSE_BIT) != 0) {
				uint bid = subchunk & ~SUBC_SPARSE_BIT;
				
				value = bid != B_AIR ? 0xffu : 0;
			} else {
				
				// subchunk id to 3d tex offset (including subchunk_size multiplication)
				// ie. split subchunk id into 3 sets of VOXTEX_TEX_SHIFT bits
				uvec3 subc_offs;
				subc_offs.x = bitfieldExtract(subchunk, VOXTEX_TEX_SHIFT*0, VOXTEX_TEX_SHIFT);
				subc_offs.y = bitfieldExtract(subchunk, VOXTEX_TEX_SHIFT*1, VOXTEX_TEX_SHIFT);
				subc_offs.z = bitfieldExtract(subchunk, VOXTEX_TEX_SHIFT*2, VOXTEX_TEX_SHIFT);
				
				texcoord = bitfieldInsert(read_pos, subc_offs, SUBCHUNK_SHIFT, 32 - SUBCHUNK_SHIFT); // (coord & SUBCHUNK_MASK) | (subc_offs << SUBCHUNK_SHIFT)
				
				uint a = uint(texelFetchOffset(voxels[1], ivec3(texcoord), 0, ivec3(0,0,0)).r != B_AIR);
				uint b = uint(texelFetchOffset(voxels[1], ivec3(texcoord), 0, ivec3(1,0,0)).r != B_AIR);
				uint c = uint(texelFetchOffset(voxels[1], ivec3(texcoord), 0, ivec3(0,1,0)).r != B_AIR);
				uint d = uint(texelFetchOffset(voxels[1], ivec3(texcoord), 0, ivec3(1,1,0)).r != B_AIR);
				uint e = uint(texelFetchOffset(voxels[1], ivec3(texcoord), 0, ivec3(0,0,1)).r != B_AIR);
				uint f = uint(texelFetchOffset(voxels[1], ivec3(texcoord), 0, ivec3(1,0,1)).r != B_AIR);
				uint g = uint(texelFetchOffset(voxels[1], ivec3(texcoord), 0, ivec3(0,1,1)).r != B_AIR);
				uint h = uint(texelFetchOffset(voxels[1], ivec3(texcoord), 0, ivec3(1,1,1)).r != B_AIR);

				uint ab = bitfieldInsert(a, b, 1, 1);
				uint cd = bitfieldInsert(c, d, 1, 1);
				uint ef = bitfieldInsert(e, f, 1, 1);
				uint gh = bitfieldInsert(g, h, 1, 1);

				uint abcd = bitfieldInsert(ab, cd, 2, 2);
				uint efgh = bitfieldInsert(ef, gh, 2, 2);

				value = bitfieldInsert(abcd, efgh, 4, 4);
			}
		}
		
		imageStore(write_mip, ivec3(pos), uvec4(value, 0u,0u,0u));
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
	
			uint a = uint(texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(0,0,0)).r != 0u);
			uint b = uint(texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(1,0,0)).r != 0u);
			uint c = uint(texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(0,1,0)).r != 0u);
			uint d = uint(texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(1,1,0)).r != 0u);
			uint e = uint(texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(0,0,1)).r != 0u);
			uint f = uint(texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(1,0,1)).r != 0u);
			uint g = uint(texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(0,1,1)).r != 0u);
			uint h = uint(texelFetchOffset(octree, ivec3(read_pos), read_mip, ivec3(1,1,1)).r != 0u);

			uint ab = bitfieldInsert(a, b, 1, 1);
			uint cd = bitfieldInsert(c, d, 1, 1);
			uint ef = bitfieldInsert(e, f, 1, 1);
			uint gh = bitfieldInsert(g, h, 1, 1);
	
			uint abcd = bitfieldInsert(ab, cd, 2, 2);
			uint efgh = bitfieldInsert(ef, gh, 2, 2);
	
			uint value = bitfieldInsert(abcd, efgh, 4, 4);

			imageStore(write_mip, ivec3(pos), uvec4(value, 0u,0u,0u));
		}
	}
#endif
