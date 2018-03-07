#version 150 core // version 3.2

$include "common.frag"

in		vec2	vs_uv;
in		vec4	vs_col;

uniform sampler2D	atlas;

void main () {
	FRAG_COL( texture(atlas, vs_uv) * vs_col );
}
