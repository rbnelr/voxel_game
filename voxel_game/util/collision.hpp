#pragma once
#include "../kissmath.hpp"

struct Ray {
	float3 pos;
	float3 dir; // normalized
};

// intersection test between circle and 1x1 square going from 0,0 to 1,1
bool circle_square_intersect (float2 circ_origin, float circ_radius);

// intersection test between cylinder and 1x1x1 cube going from 0,0,0 to 1,1,1
// cylinder origin is at the center of the circle at the base of the cylinder (-z circle)
bool cylinder_cube_intersect (float3 cyl_origin, float cyl_radius, float cyl_height);

// nearest distance from point to square (square covers [square_pos, square_pos +square_size] on each axis)
float point_square_nearest_dist (float2 square_pos, float2 square_size, float2 point);

typedef int64 voxel_coord_t;
typedef int64v3 voxel_coord;

struct VoxelRaycast {
	float		max_dist;
	voxel_coord	step_delta;
	float3		step_dist;

	float3		next;

	voxel_coord	cur_voxel;
	int			cur_axis;
	float		cur_dist;

	static int find_next_axis (float3 next) { // index of smallest component
		if (		next.x < next.y && next.x < next.z )	return 0;
		else if (	next.y < next.z )						return 1;
		else												return 2;
	}

	// Start VoxelRaycast with a ray, cur_voxel can be checked afterwards for the starting voxel
	VoxelRaycast (Ray ray, float max_dist);

	// Get the face through which the ray enters in the next step
	int get_step_face ();

	bool step ();
};

template <typename FUNC>
bool raycast_voxels (Ray ray, float max_dist, FUNC hit_block) {
	VoxelRaycast vrc = VoxelRaycast(ray, max_dist);

	if (hit_block(vrc.cur_voxel, -1, vrc.cur_dist)) // ray started inside block, -1 as no face was hit
		return true;

	while (vrc.step()) {
		if (hit_block(vrc.cur_voxel, vrc.get_step_face(), vrc.cur_dist))
			return true;
	}

	return false;
}
