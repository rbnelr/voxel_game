#version 460 core
layout(local_size_x = GROUPSZ, local_size_y = GROUPSZ, local_size_z = GROUPSZ) in;

layout(r8, binding = 4) writeonly restrict uniform image3D img;

#include "gpu_voxels.glsl"
#define INF (1. / 0.)

uniform ivec3 offset;

#define CHECK_COUNT (DF_RADIUS*2+1)

// surrounding cells sorted by distance
uniform int check_count;

struct CheckCell {
	int offs_x, offs_y, offs_z;
	float dist;
};
layout(std140, binding = 5) uniform CheckCells {
	CheckCell check_cells[CHECK_COUNT*CHECK_COUNT*CHECK_COUNT];
};

void main () {
	ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);
	pos += offset;
	
	#if 0
	float min_dist = DF_RADIUSf;
	
	for (int z=-DF_RADIUS; z<=+DF_RADIUS; ++z)
	for (int y=-DF_RADIUS; y<=+DF_RADIUS; ++y)
	for (int x=-DF_RADIUS; x<=+DF_RADIUS; ++x) {
		ivec3 offs = ivec3(x,y,z);
		
		float dist = length(vec3(max(abs(offs) - 1, 0)));
		
		if (dist < DF_RADIUSf) {
			uint val = texelFetchOffset(voxel_tex, pos, 0, offs).r;
			if (val > B_AIR) {
				min_dist = min(min_dist, dist); 
			}
		}
	}
	#else
	float min_dist = DF_RADIUSf;
	
	// iterate surrounding cells sorted by distance such that earliest soild cell must be the min distance
	for (int i=0; i<check_count; ++i) {
		ivec3 offs = ivec3(check_cells[i].offs_x, check_cells[i].offs_y, check_cells[i].offs_z);
		float dist = check_cells[i].dist;
		
		uint val = texelFetchOffset(voxel_tex, pos, 0, offs).r;
		if (val > B_AIR) {
			min_dist = dist;
			break;
		}
	}
	#endif
	
	min_dist /= DF_RADIUSf; // [0,DF_RADIUS] -> [0,1]
	
	imageStore(img, pos, vec4(min_dist, 0u,0u,0u));
}
