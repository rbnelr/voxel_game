#pragma once
#include "../kissmath.hpp"

struct Ray {
	float3 pos;
	float3 dir; // normalized
};

inline bool circle_square_intersect (float2 circ_origin, float circ_radius) { // intersection test between circle and square of edge length 1
	// square goes from 0-1 on each axis (circ_origin pos is relative to cube)
	
	float2 nearest_pos_on_square = clamp( circ_origin, 0,1 );
	
	return length_sqr(nearest_pos_on_square -circ_origin) < circ_radius*circ_radius;
}
inline bool cylinder_cube_intersect (float3 cyl_origin, float cyl_radius, float cyl_height) { // intersection test between cylinder and cube of edge length 1
	// cube goes from 0-1 on each axis (cyl_origin pos is relative to cube)
	// cylinder origin is at the center of the circle at the base of the cylinder (-z circle)
	
	if (cyl_origin.z >= 1) return false; // cylinder above cube
	if (cyl_origin.z <= -cyl_height) return false; // cylinder below cube
	
	return circle_square_intersect((float2)cyl_origin, cyl_radius);
}

inline float point_square_nearest_dist (float2 square_pos, float2 square_size, float2 point) { // nearest distance from point to square (square covers [square_pos, square_pos +square_size] on each axis)
	
	float2 pos_rel = point -square_pos;
	
	float2 nearest_pos_on_square = clamp(pos_rel, 0,square_size);
	
	return length(nearest_pos_on_square -pos_rel);
}
