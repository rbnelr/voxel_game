#version 150 core // version 3.2

$include "common.vert"

in		vec2	pos_screen;
in		vec2	uv;
in		vec4	col;

out		vec2	vs_uv;
out		vec4	vs_col;

void main () {
	vec2 pos = pos_screen / screen_dim;
	pos.y = 1 -pos.y;
	
	gl_Position =		vec4(pos * 2 -1, 0,1);
	vs_uv =				uv;
	vs_col =			col;
}
