#version 150 core // version 3.2

#define WIREFRAME 0

$include "common.frag"

in		vec3	vs_pos_cam;
in		vec4	vs_color;

void main () {
	FRAG_COL( vs_color );
}
