#version 150 core // version 3.2

$include "common.frag"

in		vec3	vs_pos_world_dir;

vec3 srgb (float r, float g, float b) {
	return pow( vec3(r,g,b) / 255.0, vec3(2.2) );
}

void main () {
	FRAG_COL( vec4(srgb(41,49,52)*8, 1) );
}
