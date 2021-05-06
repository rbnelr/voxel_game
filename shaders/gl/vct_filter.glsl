#version 460 core
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(rgba16f, binding = 0) writeonly restrict uniform image3D write_mip_col;
layout(r8, binding = 1) writeonly restrict uniform image3D write_mip_alphNX;
layout(r8, binding = 2) writeonly restrict uniform image3D write_mip_alphPX;
layout(r8, binding = 3) writeonly restrict uniform image3D write_mip_alphNY;
layout(r8, binding = 4) writeonly restrict uniform image3D write_mip_alphPY;
layout(r8, binding = 5) writeonly restrict uniform image3D write_mip_alphNZ;
layout(r8, binding = 6) writeonly restrict uniform image3D write_mip_alphPZ;

#include "common.glsl"
#include "gpu_voxels.glsl"

uniform uvec3 offsets[16];
uniform uint size;

#if MIP0
	void main () {
		uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);
		
		uint chunk_idx = pos.z / size;
		pos.z          = pos.z % size;
		
		ivec3 dst_pos = ivec3(pos + offsets[chunk_idx]);
		
		uint bid = read_bid(dst_pos);
		
		float texid = float(block_tiles[bid].sides[0]);
		vec4 col = textureLod(tile_textures, vec3(vec2(0.5), texid), 100.0);
		
		imageStore(write_mip_col, dst_pos, vec4(col.rgb * get_emmisive(bid), 0.0));
		
		
		uint bidNX = read_bid(dst_pos + ivec3(+1,0,0));
		uint bidPX = read_bid(dst_pos + ivec3(-1,0,0));
		uint bidNY = read_bid(dst_pos + ivec3(0,+1,0));
		uint bidPY = read_bid(dst_pos + ivec3(0,-1,0));
		uint bidNZ = read_bid(dst_pos + ivec3(0,0,+1));
		uint bidPZ = read_bid(dst_pos + ivec3(0,0,-1));
		
		float opacityNX = bid != B_AIR && bidNX == B_AIR ? col.a : 0.0;
		float opacityPX = bid != B_AIR && bidPX == B_AIR ? col.a : 0.0;
		float opacityNY = bid != B_AIR && bidNY == B_AIR ? col.a : 0.0;
		float opacityPY = bid != B_AIR && bidPY == B_AIR ? col.a : 0.0;
		float opacityNZ = bid != B_AIR && bidNZ == B_AIR ? col.a : 0.0;
		float opacityPZ = bid != B_AIR && bidPZ == B_AIR ? col.a : 0.0;
		
		imageStore(write_mip_alphNX, dst_pos, vec4(opacityNX, 0.0, 0.0, 0.0));
		imageStore(write_mip_alphPX, dst_pos, vec4(opacityPX, 0.0, 0.0, 0.0));
		imageStore(write_mip_alphNY, dst_pos, vec4(opacityNY, 0.0, 0.0, 0.0));
		imageStore(write_mip_alphPY, dst_pos, vec4(opacityPY, 0.0, 0.0, 0.0));
		imageStore(write_mip_alphNZ, dst_pos, vec4(opacityNZ, 0.0, 0.0, 0.0));
		imageStore(write_mip_alphPZ, dst_pos, vec4(opacityPZ, 0.0, 0.0, 0.0));
	}
#else
	uniform int read_mip;
	
	vec3 filter_rgb (sampler3D src, ivec3 src_pos) {
		vec3 a = texelFetchOffset(src, src_pos, read_mip, ivec3(0,0,0)).rgb;
		vec3 b = texelFetchOffset(src, src_pos, read_mip, ivec3(1,0,0)).rgb;
		vec3 c = texelFetchOffset(src, src_pos, read_mip, ivec3(0,1,0)).rgb;
		vec3 d = texelFetchOffset(src, src_pos, read_mip, ivec3(1,1,0)).rgb;
		vec3 e = texelFetchOffset(src, src_pos, read_mip, ivec3(0,0,1)).rgb;
		vec3 f = texelFetchOffset(src, src_pos, read_mip, ivec3(1,0,1)).rgb;
		vec3 g = texelFetchOffset(src, src_pos, read_mip, ivec3(0,1,1)).rgb;
		vec3 h = texelFetchOffset(src, src_pos, read_mip, ivec3(1,1,1)).rgb;
		
		return (((a+b) + (c+d)) + ((e+f) + (g+h))) * 0.125;
	}
	float filter_r (sampler3D src, ivec3 src_pos) {
		float a = texelFetchOffset(src, src_pos, read_mip, ivec3(0,0,0)).r;
		float b = texelFetchOffset(src, src_pos, read_mip, ivec3(1,0,0)).r;
		float c = texelFetchOffset(src, src_pos, read_mip, ivec3(0,1,0)).r;
		float d = texelFetchOffset(src, src_pos, read_mip, ivec3(1,1,0)).r;
		float e = texelFetchOffset(src, src_pos, read_mip, ivec3(0,0,1)).r;
		float f = texelFetchOffset(src, src_pos, read_mip, ivec3(1,0,1)).r;
		float g = texelFetchOffset(src, src_pos, read_mip, ivec3(0,1,1)).r;
		float h = texelFetchOffset(src, src_pos, read_mip, ivec3(1,1,1)).r;
		
		return (((a+b) + (c+d)) + ((e+f) + (g+h))) * 0.125;
	}
	
	void main () {
		uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);

		uint chunk_idx = pos.z / size;
		pos.z          = pos.z % size;
        
		if (all(lessThan(pos, uvec3(size)))) {
			ivec3 dst_pos = ivec3(pos + offsets[chunk_idx]);
			ivec3 src_pos = dst_pos * 2;
			
			imageStore(write_mip_col, dst_pos, vec4(filter_rgb(vct_col, src_pos), 0.0));
			
			imageStore(write_mip_alphNX, dst_pos, vec4(filter_r(vct_alphNX, src_pos), 0.0, 0.0, 0.0));
			imageStore(write_mip_alphPX, dst_pos, vec4(filter_r(vct_alphPX, src_pos), 0.0, 0.0, 0.0));
			imageStore(write_mip_alphNY, dst_pos, vec4(filter_r(vct_alphNY, src_pos), 0.0, 0.0, 0.0));
			imageStore(write_mip_alphPY, dst_pos, vec4(filter_r(vct_alphPY, src_pos), 0.0, 0.0, 0.0));
			imageStore(write_mip_alphNY, dst_pos, vec4(filter_r(vct_alphNZ, src_pos), 0.0, 0.0, 0.0));
			imageStore(write_mip_alphPY, dst_pos, vec4(filter_r(vct_alphPZ, src_pos), 0.0, 0.0, 0.0));
		}
	}
#endif
