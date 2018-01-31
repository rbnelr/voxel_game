#version 150 core // version 3.2

$include "common.glsl"

in		vec2	vs_uv;
in		vec4	vs_col;

uniform sampler2D	glyphs;

void main () {
	FRAG_COL( vec4(1,1,1,texture(glyphs, vs_uv).r) * vs_col );
}
