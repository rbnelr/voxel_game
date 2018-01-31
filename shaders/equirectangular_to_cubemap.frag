#version 150 core // version 3.2

$include "common.frag"

in		vec3	vs_dir_cubemap;
in		vec2	vs_pos_clip;

uniform	sampler2D	equirectangular;

#define PI 3.1415926535897932384626433832795

vec2 cubemap_dir_to_equirectangular_uv (vec3 dir) {
	return vec2(	-(atan(dir.y, dir.x) +PI/2) / (PI*2),
					-atan(length(dir.xy), dir.z) / PI );
}

void main () {
	FRAG_COL( texture(equirectangular, cubemap_dir_to_equirectangular_uv(normalize(vs_dir_cubemap))) );
}
