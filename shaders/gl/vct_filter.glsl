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
		
		//if (bid == B_MAGMA) col = vec4(0.0);
		
		imageStore(write_mip_col, dst_pos, vec4(col.rgb * get_emmisive(bid), 0.0));
		
		//float transp = col.a == 0.0 ? 0.0 : 1.0; // pretend leaves etc. are fully opaque for now
		//
		//uint bidNX = read_bid(dst_pos + ivec3(+1,0,0));
		//uint bidPX = read_bid(dst_pos + ivec3(-1,0,0));
		//uint bidNY = read_bid(dst_pos + ivec3(0,+1,0));
		//uint bidPY = read_bid(dst_pos + ivec3(0,-1,0));
		//uint bidNZ = read_bid(dst_pos + ivec3(0,0,+1));
		//uint bidPZ = read_bid(dst_pos + ivec3(0,0,-1));
		//
		//float transpNX = bid != B_AIR && bidNX == B_AIR ? transp : 0.0;
		//float transpPX = bid != B_AIR && bidPX == B_AIR ? transp : 0.0;
		//float transpNY = bid != B_AIR && bidNY == B_AIR ? transp : 0.0;
		//float transpPY = bid != B_AIR && bidPY == B_AIR ? transp : 0.0;
		//float transpNZ = bid != B_AIR && bidNZ == B_AIR ? transp : 0.0;
		//float transpPZ = bid != B_AIR && bidPZ == B_AIR ? transp : 0.0;
		//
		//imageStore(write_mip_alphNX, dst_pos, vec4(transpNX, 0.0, 0.0, 0.0));
		//imageStore(write_mip_alphPX, dst_pos, vec4(transpPX, 0.0, 0.0, 0.0));
		//imageStore(write_mip_alphNY, dst_pos, vec4(transpNY, 0.0, 0.0, 0.0));
		//imageStore(write_mip_alphPY, dst_pos, vec4(transpPY, 0.0, 0.0, 0.0));
		//imageStore(write_mip_alphNZ, dst_pos, vec4(transpNZ, 0.0, 0.0, 0.0));
		//imageStore(write_mip_alphPZ, dst_pos, vec4(transpPZ, 0.0, 0.0, 0.0));
		
		float transp = bid == B_AIR ? 0.0 : 1.0;
		
		imageStore(write_mip_alphNX, dst_pos, vec4(transp, 0.0, 0.0, 0.0));
		imageStore(write_mip_alphPX, dst_pos, vec4(transp, 0.0, 0.0, 0.0));
		imageStore(write_mip_alphNY, dst_pos, vec4(transp, 0.0, 0.0, 0.0));
		imageStore(write_mip_alphPY, dst_pos, vec4(transp, 0.0, 0.0, 0.0));
		imageStore(write_mip_alphNZ, dst_pos, vec4(transp, 0.0, 0.0, 0.0));
		imageStore(write_mip_alphPZ, dst_pos, vec4(transp, 0.0, 0.0, 0.0));
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
	
	// Filter far and close texels multiplicative
	// (0.3 transparency with 0.6 transparency behind it has combined 0.3*0.6 transparency)
	// then average the 4 computed values
	
	float filter_transpX (sampler3D src, ivec3 src_pos) {
		float t000 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,0,0)).r;
		float t100 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,0,0)).r;
		float t010 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,1,0)).r;
		float t110 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,1,0)).r;
		float t001 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,0,1)).r;
		float t101 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,0,1)).r;
		float t011 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,1,1)).r;
		float t111 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,1,1)).r;
		
		//0 1  2 3  4 5  6 7
		return 1.0 - ( t000*t100 +
		               t010*t110 +
		               t001*t101 +
		               t011*t111 ) * 0.25;
	}
	float filter_transpY (sampler3D src, ivec3 src_pos) {
		float t000 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,0,0)).r;
		float t100 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,0,0)).r;
		float t010 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,1,0)).r;
		float t110 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,1,0)).r;
		float t001 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,0,1)).r;
		float t101 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,0,1)).r;
		float t011 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,1,1)).r;
		float t111 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,1,1)).r;
		
		//0 2  1 3  4 6  5 7
		return 1.0 - ( t000*t010 +
		               t100*t110 +
		               t001*t011 +
		               t101*t111 ) * 0.25;
	}
	float filter_transpZ (sampler3D src, ivec3 src_pos) {
		float t000 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,0,0)).r;
		float t100 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,0,0)).r;
		float t010 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,1,0)).r;
		float t110 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,1,0)).r;
		float t001 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,0,1)).r;
		float t101 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,0,1)).r;
		float t011 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(0,1,1)).r;
		float t111 = 1.0 - texelFetchOffset(src, src_pos, read_mip, ivec3(1,1,1)).r;
		
		//0 4  1 5  2 6  3 7
		return 1.0 - ( t000*t001 +
		               t100*t101 +
		               t010*t011 +
		               t110*t111 ) * 0.25;
	}
	
	void main () {
		uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);

		uint chunk_idx = pos.z / size;
		pos.z          = pos.z % size;
        
		if (all(lessThan(pos, uvec3(size)))) {
			ivec3 dst_pos = ivec3(pos + offsets[chunk_idx]);
			ivec3 src_pos = dst_pos * 2;
			
			imageStore(write_mip_col, dst_pos, vec4(filter_rgb(vct_col, src_pos), 0.0));
			
			imageStore(write_mip_alphNX, dst_pos, vec4(filter_transpX(vct_alphNX, src_pos), 0.0, 0.0, 0.0));
			imageStore(write_mip_alphPX, dst_pos, vec4(filter_transpX(vct_alphPX, src_pos), 0.0, 0.0, 0.0));
			
			imageStore(write_mip_alphNY, dst_pos, vec4(filter_transpY(vct_alphNY, src_pos), 0.0, 0.0, 0.0));
			imageStore(write_mip_alphPY, dst_pos, vec4(filter_transpY(vct_alphPY, src_pos), 0.0, 0.0, 0.0));
			
			imageStore(write_mip_alphNZ, dst_pos, vec4(filter_transpZ(vct_alphNZ, src_pos), 0.0, 0.0, 0.0));
			imageStore(write_mip_alphPZ, dst_pos, vec4(filter_transpZ(vct_alphPZ, src_pos), 0.0, 0.0, 0.0));
		}
	}
#endif
