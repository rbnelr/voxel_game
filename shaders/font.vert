#version 150 core // version 3.2

in		vec2	pos_screen;
in		vec2	uv;
in		vec4	col;

out		vec2	vs_uv;
out		vec4	vs_col;

uniform	vec2	mcursor_pos;
uniform	vec2	screen_dim;

#define QUAD(a,b,c,d) b,c,a, a,c,d

const vec2 arr[6] = vec2[] (
	QUAD(	vec2(0,0),
			vec2(1,0),
			vec2(1,1),
			vec2(0,1) )
);

void main () {
	vec2 pos = pos_screen / screen_dim;
	pos.y = 1 -pos.y;
	
	gl_Position =		vec4(pos * 2 -1, 0,1);
	vs_uv =				uv;
	vs_col =			col;
}
