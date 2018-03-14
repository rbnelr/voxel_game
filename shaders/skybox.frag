#version 150 core // version 3.2

$include "common.frag"

in		vec3	vs_pos_world_dir;

vec3 srgb (float r, float g, float b) {
	return pow( vec3(r,g,b) / 255.0, vec3(2.2) );
}

void main () {
	vec3 dir = normalize(vs_pos_world_dir);
	
	vec3 sky_col =		srgb(190,239,255);
	vec3 horiz_col =	srgb(204,227,235);
	vec3 down_col =		srgb(41,49,52);
	
	vec3 col;
	if (dir.z > 0)
		col = mix(horiz_col, sky_col, dir.z);
	else
		col = mix(horiz_col, down_col, -dir.z);
	
	FRAG_COL( col );
}
