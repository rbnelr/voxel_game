#include "collision.hpp"

bool circle_square_intersect (float2 circ_origin, float circ_radius) {

	float2 nearest_pos_on_square = clamp(circ_origin, 0,1);

	return length_sqr(nearest_pos_on_square -circ_origin) < circ_radius*circ_radius;
}
bool cylinder_cube_intersect (float3 cyl_origin, float cyl_radius, float cyl_height) {

	if (cyl_origin.z >= 1) return false; // cylinder above cube
	if (cyl_origin.z <= -cyl_height) return false; // cylinder below cube

	return circle_square_intersect((float2)cyl_origin, cyl_radius);
}

float point_square_nearest_dist (float2 square_pos, float2 square_size, float2 point) {

	float2 pos_rel = point -square_pos;

	float2 nearest_pos_on_square = clamp(pos_rel, 0,square_size);

	return length(nearest_pos_on_square -pos_rel);
}

static int find_next_axis (float3 next) {
	if (		next.x < next.y && next.x < next.z )	return 0;
	else if (	next.y < next.z )						return 1;
	else												return 2;
}

VoxelRaycast::VoxelRaycast (Ray ray, float max_dist) {
	this->max_dist = max_dist;

	// get direction of each axis of ray_dir (-1, 0, +1)
	// normalize(float) is just float / abs(float)
	step_delta = voxel_coord(	(voxel_coord_t)normalize(ray.dir.x),
								(voxel_coord_t)normalize(ray.dir.y),
								(voxel_coord_t)normalize(ray.dir.z) );

	// get how far you have to travel along the ray to move by 1 unit in each axis
	// (ray_dir / abs(ray_dir.x) normalizes the ray_dir so that its x is 1 or -1
	// a zero in ray_dir produces a NaN in step because 0 / 0
	step_dist = float3(	length(ray.dir / abs(ray.dir.x)),
						length(ray.dir / abs(ray.dir.y)),
						length(ray.dir / abs(ray.dir.z)) );
	// NaN -> Inf
	step_dist = select(ray.dir != 0, step_dist, INF);

	// get initial positon in block and intial voxel coord
	float3 ray_pos_floor = floor(ray.pos);
	float3 pos_in_block = ray.pos -ray_pos_floor;

	cur_voxel = (voxel_coord)ray_pos_floor;

	// how far to step along ray to step into the next voxel for each axis
	next = step_dist * select(ray.dir > 0, 1 -pos_in_block, pos_in_block);

	// find the axis of the next voxel step
	cur_axis = find_next_axis(next);
	cur_dist = next[cur_axis];
}

int VoxelRaycast::get_step_face () {
	return cur_axis*2 +(step_delta[cur_axis] < 0 ? 1 : 0);
}

bool VoxelRaycast::step () {
	// find the axis of the cur step
	cur_axis = find_next_axis(next);
	cur_dist = next[cur_axis];

	if (cur_dist > max_dist)
		return false; // stop stepping because max_dist is reached

	// clac the distance at which the next voxel step for this axis happens
	next[cur_axis] += step_dist[cur_axis];
	// step into the next voxel
	cur_voxel[cur_axis] += step_delta[cur_axis];

	return true;
}
