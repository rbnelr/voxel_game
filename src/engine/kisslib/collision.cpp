#include "collision.hpp"
#include "debug_draw.hpp"

void AABB::dbgdraw (float3 pos, lrgba col) {
	float3 sz = hi - lo;
	float3 local_center = (lo + hi) * 0.5f;
	g_debugdraw.wire_cube(pos + local_center, sz, col);
}

bool circle_square_intersect (float2 const& circ_origin, float circ_radius) {

	float2 nearest_pos_on_square = clamp(circ_origin, 0,1);

	return length_sqr(nearest_pos_on_square -circ_origin) < circ_radius*circ_radius;
}
bool cylinder_cube_intersect (float3 const& cyl_origin, float cyl_radius, float cyl_height) {

	if (cyl_origin.z >= 1) return false; // cylinder above cube
	if (cyl_origin.z <= -cyl_height) return false; // cylinder below cube

	return circle_square_intersect((float2)cyl_origin, cyl_radius);
}

float point_square_dist (float2 const& square_pos, float2 const& square_size, float2 const& point) {

	float2 pos_rel = point -square_pos;

	float2 nearest_pos_on_square = clamp(pos_rel, 0,square_size);

	return length(nearest_pos_on_square -pos_rel);
}

float point_box_dist (float3 const& box_pos, float3 const& box_size, float3 const& point) {
	return sqrt(point_box_dist_sqr(box_pos, box_size, point));
}

float point_line_dist (float2 const& line_pos, float2 const& line_dir, float2 const& point) {

	float2 pos_rel = point - line_pos;

	float len_sqr = length_sqr(line_dir);
	float t = len_sqr == 0.0f ? 0 : dot(line_dir, pos_rel) / len_sqr;

	float2 projected = t * line_dir;
	float2 offset = pos_rel - projected;

	return length(offset);
}

float point_line_segment_dist (float2 const& line_pos, float2 const& line_dir, float2 const& point) {

	float2 pos_rel = point - line_pos;

	float len_sqr = length_sqr(line_dir);
	float t = len_sqr == 0.0f ? 0 : dot(line_dir, pos_rel) / len_sqr;

	t = clamp(t, 0.0f, 1.0f);

	float2 projected = t * line_dir;
	float2 offset = pos_rel - projected;

	return length(offset);
}

float point_box_dist_sqr (float3 const& box_pos, float3 const& box_size, float3 const& point) {

	float3 pos_rel = point - box_pos;

	float3 nearest_pos_on_square = clamp(pos_rel, 0, box_size);

	return length_sqr(nearest_pos_on_square - pos_rel);
}


// aabb gets culled when is lies completely on +normal dir side of palne
// returns true when culled
bool plane_cull_aabb (Plane const& plane, AABB const& aabb) {
	// test if any of the 9 points lie inside the plane => not culled
	float3 lo = aabb.lo - plane.pos;
	float3 hi = aabb.hi - plane.pos;

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

//bool frustrum_cull_aabb (View_Frustrum const& frust, AABB aabb) {
//	// cull if outside of one plane
//	for (int i=0; i<5; ++i) {
//		if (plane_cull_aabb(frust.planes[i], aabb))
//			return true;
//	}
//	return false;
//}

bool frustrum_cull_aabb (View_Frustrum const& frust, float lx, float ly, float lz, float hx, float hy, float hz) {
	//// Optimized version

	// cull if outside of one plane
	for (int i=0; i<5; ++i) {
		auto& plane = frust.planes[i];

		// parts of the dot products
		float dlx = plane.normal.x * (lx - plane.pos.x);
		float dly = plane.normal.y * (ly - plane.pos.y);
		float dlz = plane.normal.z * (lz - plane.pos.z);
		float dhx = plane.normal.x * (hx - plane.pos.x);
		float dhy = plane.normal.y * (hy - plane.pos.y);
		float dhz = plane.normal.z * (hz - plane.pos.z);

	#if 0
		//if ((dlx + dly + dlz) <= 0) continue;
		//if ((dhx + dly + dlz) <= 0) continue;
		//if ((dlx + dhy + dlz) <= 0) continue;
		//if ((dhx + dhy + dlz) <= 0) continue;
		//if ((dlx + dly + dhz) <= 0) continue;
		//if ((dhx + dly + dhz) <= 0) continue;
		//if ((dlx + dhy + dhz) <= 0) continue;
		//if ((dhx + dhy + dhz) <= 0) continue;

		return true; // aabb completely outside plane
	#else
		float a = (dlx + dly + dlz);
		float b = (dhx + dly + dlz);
		float c = (dlx + dhy + dlz);
		float d = (dhx + dhy + dlz);
		float e = (dlx + dly + dhz);
		float f = (dhx + dly + dhz);
		float g = (dlx + dhy + dhz);
		float h = (dhx + dhy + dhz);

	#define MIN(a,b) kissmath::min(a,b)
		float least = MIN( MIN(MIN(a,b), MIN(c,d)), MIN(MIN(e,f), MIN(g,h)) );
	#undef MIN

		if (least > 0)
			return true;// true: aabb completely outside plane
	#endif
	}
	return false;
}

bool ray_plane_intersect (Ray const& ray, Plane const& plane, float3* hit) {
	float3 offs = ray.pos - plane.pos;

	float dist   = dot(offs   , plane.normal);
	float dotdir = dot(ray.dir, plane.normal);
	
	// signs cancel out such that this works "above" or "below the plane
	float t = -dist / dotdir;

	if (t <= 0) return false; // ray points away from plane
	
	*hit = ray.dir * t + ray.pos;
	return true;
}

#if 0
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

void cylinder_cube_cast (float3 const& offset, float3 const& dir, float cyl_r, float cyl_h, CollisionHit* hit) {
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
#endif

bool ray_line_closest_intersect (float3 const& ray_pos, float3 const& ray_dir, float3 const& line_pos, float3 const& line_dir,
	float3* intersect) {

	float3 cross_dir = cross(ray_dir, line_dir);
	float divisor = length_sqr(cross_dir);

	if (divisor == 0.0f)
		return false; // target ray and axis line are exactly parallel

	float inv = 1.0f / divisor;
	float3 rel = line_pos - ray_pos;

	float3x3 m;
	m.arr[0] = rel;
	m.arr[1] = line_dir;
	m.arr[2] = cross_dir;
	float t_ray = determinant(m);
	t_ray *= inv;

	if (t_ray < 0.0f)
		return false; // target ray points away from axis line (intersection would be backwards on the ray (behind the camera))

	m.arr[1] = ray_dir;
	float t_line = determinant(m);
	t_line *= inv;

	*intersect = line_pos + line_dir * t_line;
	return true;
}
