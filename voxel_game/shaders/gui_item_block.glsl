#version 330 core

$include "common.glsl"

$if vertex
	layout (location = 0) in vec3	pos;
	layout (location = 1) in vec3	normal;
	layout (location = 2) in vec2	uv;
	layout (location = 3) in float	tex_indx;
	
	out vec3	vs_normal;
	out vec2	vs_uv;
	out float	vs_tex_indx;

	void main () {
		gl_Position =		vec4(pos.xy / viewport_size * 2 - 1, 0, 1);

		vs_normal =			normal;
		vs_uv =		        uv;
		vs_tex_indx =		tex_indx;
	}
$endif

$if fragment
	in vec3		vs_normal;
	in vec2	    vs_uv;
	in float	vs_tex_indx;

	uniform	sampler2DArray tile_textures;

	uniform bool alpha_test;
	#define ALPHA_TEST_THRES 127.0

	out vec4 frag_col;

	void main () {
		vec4 col = texture(tile_textures, vec3(vs_uv, vs_tex_indx));
		col.rgb *= dot(normalize(vs_normal), normalize(vec3(0.1,0.4,1)));

		if (alpha_test) {
			if (col.a <= ALPHA_TEST_THRES / 255.0)
				discard;
			col.a = 1.0;
		}

		frag_col = col;
	}
$endif
