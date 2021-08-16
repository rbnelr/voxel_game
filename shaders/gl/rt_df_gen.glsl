#version 460 core
layout(local_size_x = GROUPSZ, local_size_y = GROUPSZ, local_size_z = GROUPSZ) in;

#include "gpu_voxels.glsl"

#if   PASS == 0
	layout(r8i , binding = 5) writeonly restrict uniform iimage3D result;
#elif PASS == 1
	layout(r8i , binding = 4) readonly  restrict uniform iimage3D input_offs;
	layout(rg8i, binding = 5) writeonly restrict uniform iimage3D result;
#else
	layout(rg8i, binding = 4) readonly  restrict uniform iimage3D input_offs;
	layout(r8ui, binding = 5) writeonly restrict uniform uimage3D result;
#endif

#define INF (1. / 0.)
#define iINF 0x7fffffff

uniform ivec3 offset;
uniform int dispatch_id;

void main () {
	ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);
	ivec3 temp_pos = pos + ivec3(0,0, dispatch_id*CHUNK_SIZE);
	ivec3 world_pos = pos + offset;
	
#if PASS == 0
	
	int min_dist = iINF;
	int min_offs = iINF;
	
	int start = max(pos.x - DF_RADIUS, 0);
	int end   = max(pos.x + DF_RADIUS, 0);
	for (int loc=start; loc<=end; ++loc) {
		int offs = loc - pos.x;
		int dist = abs(offs);
		uint bid = texelFetchOffset(voxel_tex, world_pos, 0, ivec3(offs,0,0)).r;
		if (dist < min_dist && bid > B_AIR) {
			min_dist = dist;
			min_offs = offs;
		}
	}
	
	imageStore(result, temp_pos, ivec4(min_offs, 0,0,0));
	
#elif PASS == 1
	
	int min_dist = iINF;
	ivec2 min_offs = ivec2(iINF);
	
	int start = max(pos.y - DF_RADIUS, 0);
	int end   = max(pos.y + DF_RADIUS, 0);
	for (int loc=start; loc<=end; ++loc) {
		int offsY = loc - pos.y;
		int offsX = imageLoad(input_offs, temp_pos + ivec3(0,offsY,0)).r;
		int dist = offsX*offsX + offsY*offsY;
		if (dist < min_dist) {
			min_dist = dist;
			min_offs = ivec2(offsX, offsY);
		}
	}
	
	imageStore(result, temp_pos, ivec4(min_offs, 0,0));
#else
	ivec2 offsXY = imageLoad(input_offs, temp_pos).rg;
	
	float min_dist = length(vec2(offsXY));
	
	min_dist /= DF_RADIUSf; // [0,DF_RADIUS] -> [0,1]
	
	uint val = uint(clamp(min_dist, 0.0, 1.0) * 255);
	
	imageStore(result, world_pos, uvec4(val, 0u,0u,0u));
#endif
}
