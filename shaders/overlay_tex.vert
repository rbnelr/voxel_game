#version 150 core // version 3.2

out		vec2	vs_uv;

uniform	vec2	mcursor_pos;
uniform	vec2	screen_dim;

uniform	vec2	pos_clip;
uniform	vec2	size_clip;

#define QUAD(a,b,c,d) b,c,a, a,c,d

const vec2 UV[6] = vec2[] (
	QUAD(	vec2(0,0),
			vec2(1,0),
			vec2(1,1),
			vec2(0,1) )
);

void main () {
	gl_Position =		vec4(pos_clip +UV[gl_VertexID] * size_clip, 0,1);
	vs_uv =				UV[gl_VertexID];
}
