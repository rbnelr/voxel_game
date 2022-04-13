#version 460 core
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

#include "common.glsl"
#include "gpu_voxels.glsl"

uniform uvec3 offsets[16];
uniform uint size;

//https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
vec3 to_srgb (vec3 lrgb) {
	bvec3 cutoff = lessThan(lrgb, vec3(0.0031308));
	vec3 higher = vec3(1.055) * pow(lrgb, vec3(1.0/2.4)) - vec3(0.055);
	vec3 lower = lrgb * vec3(12.92);
	return mix(higher, lower, cutoff);
}

// pack my raw lighting values into SRGB8_ALPHA8 texels manually
// since imageStore do not support srgb for some reason
uvec4 pack_texel (vec4 lrgba) {
	lrgba.rgb /= VCT_COL_MAX; // compress out light range into [0,1]
	
	lrgba = clamp(lrgba, vec4(0.0), vec4(1.0)); // clamp for good measure
	
	lrgba.rgb = to_srgb(lrgba.rgb); // linear -> srgb
	
	// TODO: is [0,1] -> floored [0,256) more correct?
	uvec4 rgba8 = uvec4(round(lrgba * vec4(255.0))); // [0,1] -> rounded [0,255]
	return rgba8;
}

layout(rgba8ui, binding = 0) writeonly restrict uniform uimage3D write_mipNX;
layout(rgba8ui, binding = 1) writeonly restrict uniform uimage3D write_mipPX;
layout(rgba8ui, binding = 2) writeonly restrict uniform uimage3D write_mipNY;
layout(rgba8ui, binding = 3) writeonly restrict uniform uimage3D write_mipPY;
layout(rgba8ui, binding = 4) writeonly restrict uniform uimage3D write_mipNZ;
layout(rgba8ui, binding = 5) writeonly restrict uniform uimage3D write_mipPZ;

#if MIP0
	void main () {
		uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);
		
		uint chunk_idx = pos.z / size;
		pos.z          = pos.z % size;
		
		ivec3 dst_pos = ivec3(pos + offsets[chunk_idx]);
		
		uint bid = texelFetch(voxel_tex, dst_pos, 0).r;
		
		float texid = float(block_tiles[bid].sides[0]);
		vec4 col = textureLod(tile_textures, vec3(vec2(0.5), texid), 100.0);
		
		//if (bid == B_MAGMA) col = vec4(0.0);
		//if (bid == B_GLOWSHROOM) col = vec4(0.0);
		
		
		
		//uint bids[3][3][3];
		//for (int z=0; z<3; ++z)
		//for (int y=0; y<3; ++y)
		//for (int x=0; x<3; ++x)
		//	bids[z][y][x] = texelFetch(voxel_tex, dst_pos + ivec3(x-1,y-1,z-1), 0).r;
		
		float alpha = 0.0;
		vec3 emissive = vec3(0.0);
		
		if (bid != B_AIR) {
			alpha = get_fake_alpha(bid);
			emissive = col.rgb * get_emmisive(bid);
			
			//emissive += col.rgb * 0.2; // makes everything glow, for testing
		}
		
		
		//bool visible = false;
		//for (int z=0; z<3; ++z)
		//for (int y=0; y<3; ++y)
		//for (int x=0; x<3; ++x)
		//	if (x!=1 || y!=1 || z!=1)
		//		visible = visible || bids[z][y][x] == B_AIR;
		
		//bool visible =
		//	(bids[1][1][0] == B_AIR) || (bids[1][1][2] == B_AIR) ||
		//	(bids[1][0][1] == B_AIR) || (bids[1][2][1] == B_AIR) ||
		//	(bids[0][1][1] == B_AIR) || (bids[2][1][1] == B_AIR);
		
		//if (!visible) emissive = vec3(0.0);
		//if (!visible) alpha = 0.0;
		
		imageStore(write_mipNX, dst_pos, pack_texel(vec4(emissive, alpha)));
		imageStore(write_mipPX, dst_pos, pack_texel(vec4(emissive, alpha)));
		imageStore(write_mipNY, dst_pos, pack_texel(vec4(emissive, alpha)));
		imageStore(write_mipPY, dst_pos, pack_texel(vec4(emissive, alpha)));
		imageStore(write_mipNZ, dst_pos, pack_texel(vec4(emissive, alpha)));
		imageStore(write_mipPZ, dst_pos, pack_texel(vec4(emissive, alpha)));
	}
#else
	uniform int read_mip;
	
	uniform int src_width;
	uniform int dst_width;
	
	// Preintegration - filter down 3d texture for 6 view directions
	
	// Rather than simply averaging the 6 textures of the previous mip do the following:
	// sample the 4 close and the corresponding 4 far texels
	// (write_mipNX is the one that gets sampled later if cones go in the negative x direction)
	// blend each pair of texels as viewed from the direction then average the 4 resulting values
	// this makes a 1x2x2 wall have the 'correct' values of 1/2 opacity or 1 opacity depending on view direction
	// it also makes it so that light should not leak though thin walls in a lot of cases (I think)
	
	vec4 preintegrate ( vec4 a0, vec4 a1,  vec4 b0, vec4 b1,
	                    vec4 c0, vec4 c1,  vec4 d0, vec4 d1 ) {
		// alpha is 1-alpha
		vec4 a,b,c,d;
		
		// alpha actually getting through
		a.a = a0.a * a1.a;
		b.a = b0.a * b1.a;
		c.a = c0.a * c1.a;
		d.a = d0.a * d1.a;
		
		// color is front + back getting through front alpha
		a.rgb = a0.rgb + (a1.rgb * a0.a);
		b.rgb = b0.rgb + (b1.rgb * b0.a);
		c.rgb = c0.rgb + (c1.rgb * c0.a);
		d.rgb = d0.rgb + (d1.rgb * d0.a);
		
		return (a+b+c+d) * 0.25;
	}
	
	vec4 _load (vec4 col) {
		col = col * VCT_UNPACK;
		col.a = 1.0 - col.a;
		return col;
	}
	uvec4 _store (vec4 col) {
		col.a = 1.0 - col.a;
		return pack_texel(col);
	}
	
	#define LOAD(src, mip) \
		vec4 a = _load(texelFetchOffset(src, src_pos, mip, ivec3(0,0,0))); \
		vec4 b = _load(texelFetchOffset(src, src_pos, mip, ivec3(1,0,0))); \
		vec4 c = _load(texelFetchOffset(src, src_pos, mip, ivec3(0,1,0))); \
		vec4 d = _load(texelFetchOffset(src, src_pos, mip, ivec3(1,1,0))); \
		vec4 e = _load(texelFetchOffset(src, src_pos, mip, ivec3(0,0,1))); \
		vec4 f = _load(texelFetchOffset(src, src_pos, mip, ivec3(1,0,1))); \
		vec4 g = _load(texelFetchOffset(src, src_pos, mip, ivec3(0,1,1))); \
		vec4 h = _load(texelFetchOffset(src, src_pos, mip, ivec3(1,1,1)));
		
	#define STORE(dst, val) imageStore(dst, dst_pos, _store(val))
	
	void main () {
		uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);

		uint chunk_idx = pos.z / size;
		pos.z          = pos.z % size;
        
		if (all(lessThan(pos, uvec3(size)))) {
			ivec3 dst_pos = ivec3(pos + offsets[chunk_idx]);
			ivec3 src_pos = dst_pos * 2;
			
		#if 0
			{
				LOAD(vct_texNX, read_mip)
				STORE(write_mipNX, preintegrate(b,a, d,c, f,e, h,g));
			}
			{
				LOAD(vct_texPX, read_mip)
				STORE(write_mipPX, preintegrate(a,b, c,d, e,f, g,h));
			}
			
			{
				LOAD(vct_texNY, read_mip)
				STORE(write_mipNY, preintegrate(c,a, d,b, g,e, h,f));
			}
			{
				LOAD(vct_texPY, read_mip)
				STORE(write_mipPY, preintegrate(a,c, b,d, e,g, f,h));
			}
			
			{
				LOAD(vct_texNZ, read_mip)
				STORE(write_mipNZ, preintegrate(e,a, f,b, g,c, h,d));
			}
			{
				LOAD(vct_texPZ, read_mip)
				STORE(write_mipPZ, preintegrate(a,e, b,f, c,g, d,h));
			}
		#else
			{
				LOAD(vct_texPZ, read_mip)
				STORE(write_mipNX, (a+e+b+f+c+g+d+h)/8.0);
				//STORE(write_mipPX, (a+e+b+f+c+g+d+h)/8.0);
				//STORE(write_mipNY, (a+e+b+f+c+g+d+h)/8.0);
				//STORE(write_mipPY, (a+e+b+f+c+g+d+h)/8.0);
				//STORE(write_mipNZ, (a+e+b+f+c+g+d+h)/8.0);
				//STORE(write_mipPZ, (a+e+b+f+c+g+d+h)/8.0);
			}
		#endif
		}
	}
#endif
