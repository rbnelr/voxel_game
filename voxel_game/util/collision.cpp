#include "collision.hpp"
#include "../graphics/camera.hpp"

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

float point_box_nearest_dist (float3 box_pos, float3 box_size, float3 point) {

	float3 pos_rel = point - box_pos;

	float3 nearest_pos_on_square = clamp(pos_rel, 0, box_size);

	return length(nearest_pos_on_square - pos_rel);
}

// aabb gets culled when is lies completely on +normal dir side of palne
// returns true when culled
bool plane_cull_aabb (Plane const& plane, AABB aabb) {
	// test if any of the 9 points lie inside the plane => not culled
	aabb.lo -= plane.pos;
	aabb.hi -= plane.pos;

	if (dot(plane.normal, float3(aabb.lo.x, aabb.lo.y, aabb.lo.z)) <= 0) return false;
	if (dot(plane.normal, float3(aabb.hi.x, aabb.lo.y, aabb.lo.z)) <= 0) return false;
	if (dot(plane.normal, float3(aabb.lo.x, aabb.hi.y, aabb.lo.z)) <= 0) return false;
	if (dot(plane.normal, float3(aabb.hi.x, aabb.hi.y, aabb.lo.z)) <= 0) return false;
	if (dot(plane.normal, float3(aabb.lo.x, aabb.lo.y, aabb.hi.z)) <= 0) return false;
	if (dot(plane.normal, float3(aabb.hi.x, aabb.lo.y, aabb.hi.z)) <= 0) return false;
	if (dot(plane.normal, float3(aabb.lo.x, aabb.hi.y, aabb.hi.z)) <= 0) return false;
	if (dot(plane.normal, float3(aabb.hi.x, aabb.hi.y, aabb.hi.z)) <= 0) return false;

	return true;
}

bool frustrum_cull_aabb (View_Frustrum const& frust, AABB aabb) {
	// cull if outside of one plane
	for (int i=0; i<6; ++i) {
		if (plane_cull_aabb(frust.planes[i], aabb))
			return true;
	}
	return false;
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

	// get initial positon in block and intial voxel coord
	float3 ray_pos_floor = floor(ray.pos);
	float3 pos_in_block = ray.pos -ray_pos_floor;

	cur_voxel = (voxel_coord)ray_pos_floor;

	// how far to step along ray to step into the next voxel for each axis
	next = step_dist * select(ray.dir > 0, 1 -pos_in_block, pos_in_block);

	// NaN -> Inf
	next = select(ray.dir != 0, next, INF);

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

////

// raycast against yz aligned plane from (pos_x, 0, -height) to (pos_x, 1, 1)
void _minkowski_cylinder_cube__raycast_x_plane (
		float3 ray_pos, float3 ray_dir,
		float plane_x, float normal_x, float height,
		CollisionHit* hit) {
	float delta_x = plane_x - ray_pos.x;
	
	if (ray_dir.x * delta_x < 0 || ray_dir.x * normal_x >= 0) return; // ray parallel to plane or ray points away from plane or going through backside of plane

	float2 delta_yz = delta_x * (float2(ray_dir.y,ray_dir.z) / ray_dir.x);

	float2 hit_pos_yz = float2(ray_pos.y,ray_pos.z) + delta_yz;

	if (!all(hit_pos_yz > float2(0,-height) && hit_pos_yz < 1)) return;

	float hit_dist = length(float3(delta_x, delta_yz[0], delta_yz[1]));

	if (hit_dist < hit->dist) {
		hit->dist = hit_dist;
		hit->pos = float3(plane_x, hit_pos_yz[0], hit_pos_yz[1]);
		hit->normal = float3(normal_x,0,0);
	}
}
// raycast against xz aligned plane from (0, pos_y, -height) to (1, pos_y, 1)
void _minkowski_cylinder_cube__raycast_y_plane (
		float3 ray_pos, float3 ray_dir,
		float plane_y, float normal_y, float height,
		CollisionHit* hit) {
	float delta_y = plane_y - ray_pos.y;
	
	if (ray_dir.y * delta_y < 0 || ray_dir.y * normal_y >= 0) return; // ray parallel to plane or ray points away from plane or going through backside of plane

	float2 delta_xz = delta_y * (float2(ray_dir.x,ray_dir.z) / ray_dir.y);

	float2 hit_pos_xz = float2(ray_pos.x,ray_pos.z) + delta_xz;

	if (!all(hit_pos_xz > float2(0,-height) && hit_pos_xz < 1)) return;

	float hit_dist = length(float3(delta_xz[0], delta_y, delta_xz[1]));

	if (hit_dist < hit->dist) {
		hit->dist = hit_dist;
		hit->pos = float3(hit_pos_xz[0], plane_y, hit_pos_xz[1]);
		hit->normal = float3(0,normal_y,0);
	}
}
// raycast against cylinder side ie. cylinder with axis z and radius cyl_r from z -height to 1
//  NOTE: cylinder caps are not handled here but instead count for _minkowski_cylinder_cube__raycast_cap_plane
void _minkowski_cylinder_cube__raycast_cylinder_side (
		float3 ray_pos, float3 ray_dir,
		float2 cyl_pos2d, float height, float cyl_r,
		CollisionHit* hit) {
	// do 2d circle raycase using on xy plane
	float ray_dir2d_len = length((float2)ray_dir);
	if (ray_dir2d_len == 0) return; // ray parallel to cylinder
	float2 unit_ray_dir2d = (float2)ray_dir / ray_dir2d_len;

	float2 circ_rel_p = cyl_pos2d - (float2)ray_pos;

	// dist along ray to closest approach with cylinder circle 
	float closest_p_dist = dot(unit_ray_dir2d, circ_rel_p);
	float2 closest_p = unit_ray_dir2d * closest_p_dist;

	// vector from cyl circle to closest point
	float2 circ_to_closest = closest_p - circ_rel_p;

	float r_sqr = cyl_r * cyl_r;
	float dist_sqr = length_sqr(circ_to_closest);

	if (dist_sqr >= r_sqr) return; // ray does not cross cylinder

	// calc ray circle entry and exit distances (in the 2d version of the ray)
	float chord_half_length = sqrt( r_sqr - dist_sqr );
	float closest_hit_dist2d = closest_p_dist - chord_half_length;
	//float furthest_hit_dist2d = closest_p_dist + chord_half_length;
	
	float hit_dist2d;
	if (closest_hit_dist2d >= 0)		hit_dist2d = closest_hit_dist2d;
	//else if (furthest_hit_dist2d >= 0)	hit_dist2d = furthest_hit_dist2d; // hit cylinder from the inside
	else								return; // circle hit is on backwards direction of ray, ie. no hit

	// get xy delta to hit
	float2 delta_xy = hit_dist2d * unit_ray_dir2d;

	// calc hit z delta
	float delta_z = length(delta_xy) * (ray_dir.z / ray_dir2d_len);

	float3 delta = float3(delta_xy, delta_z); // relative to ray
	float3 hit_pos = ray_pos + delta; // relative to ray

	if (!(hit_pos.z > -height && hit_pos.z < 1)) return; // ray above or below cylinder (cap is not handled here, but instead in _minkowski_cylinder_cube__raycast_cap_plane)

	float dist = length(delta);

	if (dist < hit->dist) {
		hit->dist = dist;
		hit->pos = hit_pos;
		hit->normal = float3(normalize(delta_z - circ_rel_p), 0);
	}
}
// raycast against xy aligned square with rounded corners ie. (0,0) to (1,1) square + radius
void _minkowski_cylinder_cube__raycast_cap_plane (
		float3 ray_pos, float3 ray_dir,
		float plane_z, float normal_z, float radius,
		CollisionHit* hit) {
	// normal axis aligned plane raycast
	float delta_z = plane_z -ray_pos.z;

	if (ray_dir.z * delta_z < 0 || ray_dir.z * normal_z >= 0) return; // if ray parallel to plane or ray points away from plane or going through backside of plane

	float2 delta_xy = delta_z * (((float2)ray_dir) / ray_dir.z);

	float2 plane_hit_xy = (float2)ray_pos + delta_xy;

	// check if cylinder base/top circle cap intersects with block top/bottom square
	float2 closest_p = clamp(plane_hit_xy, 0,1);

	float dist_sqr = length_sqr(closest_p -plane_hit_xy);
	if (dist_sqr >= radius*radius) return; // hit outside of square + radius

	float hit_dist = length(float3(delta_xy, delta_z));

	if (hit_dist < hit->dist) {
		hit->dist = hit_dist;
		hit->pos = float3(plane_hit_xy, plane_z);
		hit->normal = float3(0,0, normal_z);
	}
}

void cylinder_cube_cast (float3 offset, float3 dir, float cyl_r, float cyl_h, CollisionHit* hit) {
	// this geometry we are raycasting onto represents the minowski sum of the cube and the cylinder

	_minkowski_cylinder_cube__raycast_cap_plane(offset, dir, 1,		   +1, cyl_r, hit); // block top
	_minkowski_cylinder_cube__raycast_cap_plane(offset, dir, -cyl_h,   -1, cyl_r, hit); // block bottom

	_minkowski_cylinder_cube__raycast_x_plane(  offset, dir,   -cyl_r, -1, cyl_h, hit); // block -X
	_minkowski_cylinder_cube__raycast_x_plane(  offset, dir, 1 +cyl_r, +1, cyl_h, hit); // block +X
	_minkowski_cylinder_cube__raycast_y_plane(  offset, dir,   -cyl_r, -1, cyl_h, hit); // block -Y
	_minkowski_cylinder_cube__raycast_y_plane(  offset, dir, 1 +cyl_r, +1, cyl_h, hit); // block +Y

	_minkowski_cylinder_cube__raycast_cylinder_side( offset, dir, float2( 0, 0), cyl_h, cyl_r, hit); // block rouned edge
	_minkowski_cylinder_cube__raycast_cylinder_side( offset, dir, float2( 0,+1), cyl_h, cyl_r, hit); // block rouned edge
	_minkowski_cylinder_cube__raycast_cylinder_side( offset, dir, float2(+1, 0), cyl_h, cyl_r, hit); // block rouned edge
	_minkowski_cylinder_cube__raycast_cylinder_side( offset, dir, float2(+1,+1), cyl_h, cyl_r, hit); // block rouned edge
}
