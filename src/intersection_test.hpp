
static bool circle_square_intersect (fv2 circ_origin, f32 circ_radius) { // intersection test between circle and square of edge length 1
	// square goes from 0-1 on each axis (circ_origin pos is relative to cube)
	
	fv2 nearest_pos_on_square = clamp( circ_origin, 0,1 );
	
	return length_sqr(nearest_pos_on_square -circ_origin) < circ_radius*circ_radius;
}
static bool cylinder_cube_intersect (fv3 cyl_origin, f32 cyl_radius, f32 cyl_height) { // intersection test between cylinder and cube of edge length 1
	// cube goes from 0-1 on each axis (cyl_origin pos is relative to cube)
	// cylinder origin is at the center of the circle at the base of the cylinder (-z circle)
	
	if (cyl_origin.z >= 1) return false; // cylinder above cube
	if (cyl_origin.z <= -cyl_height) return false; // cylinder below cube
	
	return circle_square_intersect(cyl_origin.xy(), cyl_radius);
}

static f32 point_square_nearest_dist (fv2 square_pos, fv2 square_size, fv2 point) { // nearest distance from point to square (square covers [square_pos, square_pos +square_size] on each axis)
	
	fv2 pos_rel = point -square_pos;
	
	fv2 nearest_pos_on_square = clamp(pos_rel, 0,square_size);
	
	return length(nearest_pos_on_square -pos_rel);
}
