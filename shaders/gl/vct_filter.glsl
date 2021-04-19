#version 460 core
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(rgba16f, binding = 4) writeonly restrict uniform image3D write_mip;

#include "common.glsl"
#include "gpu_voxels.glsl"

uniform uvec3 offsets[16];
uniform uint size;

#if MIP0
	vec4 get_value (uint bid) {
		float texid = float(block_tiles[bid].sides[0]);
		vec4 col = textureLod(tile_textures, vec3(vec2(0.5), texid), 100.0);
		float emmiss = get_emmisive(bid);
		float opacity = bid == B_AIR ? 0.0 : col.a;
		
		return vec4(col.rgb * emmiss, opacity);
	}
	void main () {
		uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);
		
		uint chunk_idx = pos.z / size;
		pos.z          = pos.z % size;
		
		pos += offsets[chunk_idx];
		
		uint bid = read_bid(ivec3(pos));
		vec4 value = get_value(bid);
		
		imageStore(write_mip, ivec3(pos), value);
	}
#else
	uniform int read_mip;

	void main () {
		uvec3 pos = uvec3(gl_GlobalInvocationID.xyz);

		uint chunk_idx = pos.z / size;
		pos.z          = pos.z % size;

		if (all(lessThan(pos, uvec3(size)))) {
			pos += offsets[chunk_idx];
			
			vec3 read_pos = vec3(pos) * 2.0 + 1.0;
			vec3 uv = read_pos / textureSize(vct_tex, read_mip);

			vec4 emiss = textureLod(vct_tex, uv, read_mip);
			imageStore(write_mip, ivec3(pos), emiss);
		}
	}
#endif
