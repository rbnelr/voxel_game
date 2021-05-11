#version 460 core
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

#include "common.glsl"
#include "gpu_voxels.glsl"

uniform uvec3 offsets[16];
uniform uint size;

layout(rgba16f, binding = 0) writeonly restrict uniform image3D write_mipNX;
layout(rgba16f, binding = 1) writeonly restrict uniform image3D write_mipPX;
layout(rgba16f, binding = 2) writeonly restrict uniform image3D write_mipNY;
layout(rgba16f, binding = 3) writeonly restrict uniform image3D write_mipPY;
layout(rgba16f, binding = 4) writeonly restrict uniform image3D write_mipNZ;
layout(rgba16f, binding = 5) writeonly restrict uniform image3D write_mipPZ;

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
		
		uint bidNX = read_bid(dst_pos + ivec3(-1,0,0));
		uint bidPX = read_bid(dst_pos + ivec3(+1,0,0));
		uint bidNY = read_bid(dst_pos + ivec3(0,-1,0));
		uint bidPY = read_bid(dst_pos + ivec3(0,+1,0));
		uint bidNZ = read_bid(dst_pos + ivec3(0,0,-1));
		uint bidPZ = read_bid(dst_pos + ivec3(0,0,+1));
		
		bool blocked =
			(bidNX != B_AIR) && (bidPX != B_AIR) &&
			(bidNY != B_AIR) && (bidPY != B_AIR) &&
			(bidNZ != B_AIR) && (bidPZ != B_AIR);
		
		vec4 val;
		val.rgb = blocked ? vec3(0.0) : col.rgb * get_emmisive(bid);
		val.a = bid != B_AIR ? 1.0 : 0.0;
		
		imageStore(write_mipNX, dst_pos, val);
		imageStore(write_mipPX, dst_pos, val);
		imageStore(write_mipNY, dst_pos, val);
		imageStore(write_mipPY, dst_pos, val);
		imageStore(write_mipNZ, dst_pos, val);
		imageStore(write_mipPZ, dst_pos, val);
		
		//imageStore(write_mipNX, dst_pos, vec4(val.rgb, bid != B_AIR && bidPX == B_AIR ? 1.0 : 0.0));
		//imageStore(write_mipPX, dst_pos, vec4(val.rgb, bid != B_AIR && bidNX == B_AIR ? 1.0 : 0.0));
		//imageStore(write_mipNY, dst_pos, vec4(val.rgb, bid != B_AIR && bidPY == B_AIR ? 1.0 : 0.0));
		//imageStore(write_mipPY, dst_pos, vec4(val.rgb, bid != B_AIR && bidNY == B_AIR ? 1.0 : 0.0));
		//imageStore(write_mipNZ, dst_pos, vec4(val.rgb, bid != B_AIR && bidPZ == B_AIR ? 1.0 : 0.0));
		//imageStore(write_mipPZ, dst_pos, vec4(val.rgb, bid != B_AIR && bidNZ == B_AIR ? 1.0 : 0.0));
	}
#else
	
	uniform int read_mip;
	
	// Preintegration - filter down 3d texture for 6 view directions
	
	// Rather than simply averaging the 6 textures of the previous mip do the following:
	// sample the 4 close and the corresponding 4 far texels
	// (write_mipNX is the one that gets sampled later if cones go in the negative x direction)
	// blend each pair of texels as viewed from the direction then average the 4 resulting values
	// this makes a 1x2x2 wall have the 'correct' values of 1/2 opacity or 1 opacity depending on view direction
	// it also makes it so that light should not leak though thin walls in a lot of cases (I think)
	
	vec4 preintegrate ( vec4 a0, vec4 a1,  vec4 b0, vec4 b1,
	                    vec4 c0, vec4 c1,  vec4 d0, vec4 d1 ) {
		vec4 a,b,c,d;
		
		a.a = 1.0 - ((1.0 - a0.a) * (1.0 - a1.a)); // alpha actually getting through
		a.rgb  = a0.rgb + (a1.rgb * (1.0 - a0.a)); // color is front + back getting through front alpha
		
		b.a = 1.0 - ((1.0 - b0.a) * (1.0 - b1.a));
		b.rgb  = b0.rgb + (b1.rgb * (1.0 - b0.a));
		
		c.a = 1.0 - ((1.0 - c0.a) * (1.0 - c1.a));
		c.rgb  = c0.rgb + (c1.rgb * (1.0 - c0.a));
		
		d.a = 1.0 - ((1.0 - d0.a) * (1.0 - d1.a));
		d.rgb  = d0.rgb + (d1.rgb * (1.0 - d0.a));
		
		return ((a+b) + (c+d)) * 0.25;
	}
	
	#define READ(src, mip) \
		vec4 a = texelFetchOffset(src, src_pos, mip, ivec3(0,0,0)); \
		vec4 b = texelFetchOffset(src, src_pos, mip, ivec3(1,0,0)); \
		vec4 c = texelFetchOffset(src, src_pos, mip, ivec3(0,1,0)); \
		vec4 d = texelFetchOffset(src, src_pos, mip, ivec3(1,1,0)); \
		vec4 e = texelFetchOffset(src, src_pos, mip, ivec3(0,0,1)); \
		vec4 f = texelFetchOffset(src, src_pos, mip, ivec3(1,0,1)); \
		vec4 g = texelFetchOffset(src, src_pos, mip, ivec3(0,1,1)); \
		vec4 h = texelFetchOffset(src, src_pos, mip, ivec3(1,1,1));
	
	void main () {
		uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);

		uint chunk_idx = pos.z / size;
		pos.z          = pos.z % size;
        
		if (all(lessThan(pos, uvec3(size)))) {
			ivec3 dst_pos = ivec3(pos + offsets[chunk_idx]);
			ivec3 src_pos = dst_pos * 2;
		
		{
			READ(vct_texNX, read_mip)
			imageStore(write_mipNX, dst_pos, preintegrate(b,a, d,c, f,e, h,g));
		}
		{
			READ(vct_texPX, read_mip)
			imageStore(write_mipPX, dst_pos, preintegrate(a,b, c,d, e,f, g,h));
		}
		
		{
			READ(vct_texNY, read_mip)
			imageStore(write_mipNY, dst_pos, preintegrate(c,a, d,b, g,e, h,f));
		}
		{
			READ(vct_texPY, read_mip)
			imageStore(write_mipPY, dst_pos, preintegrate(a,c, b,d, e,g, f,h));
		}
		
		{
			READ(vct_texNZ, read_mip)
			imageStore(write_mipNZ, dst_pos, preintegrate(e,a, f,b, g,c, h,d));
		}
		{
			READ(vct_texPZ, read_mip)
			imageStore(write_mipPZ, dst_pos, preintegrate(a,e, b,f, c,g, d,h));
		}
		}
	}
#endif
