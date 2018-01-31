#version 150 core // version 3.2

$include "common.glsl"

in		vec2	vs_uv;

uniform samplerCube	tex0;

vec3 uv_to_cubemap_dir (vec2 uv) {
	return Z_UP_CONVENTION_TO_OPENGL_CUBEMAP_CONVENTION * vec3(
		sin(uv.x * 2*PI -PI) * cos(uv.y * PI -PI/2),
		cos(uv.x * 2*PI -PI) * cos(uv.y * PI -PI/2),
		sin(uv.y * PI -PI/2) );
}

void main () {
	vec3 dir = uv_to_cubemap_dir(vs_uv);
	FRAG_COL( texture(tex0, dir).rgba );
}
