#version 150 core // version 3.2

$include "common.glsl"

in		vec2	vs_uv;

uniform sampler2D	tex0;

void main () {
	FRAG_COL( texture(tex0, vs_uv).rgba );
}
