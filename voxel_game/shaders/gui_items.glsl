#version 330 core

$if vertex
	layout (location = 0) in vec4	pos_clip;
	layout (location = 1) in vec2	uv;
	layout (location = 2) in float	tex_indx;
	layout (location = 3) in float	brightness;

	out vec2	vs_uv;
	out float	vs_tex_indx;
	out float	vs_brightness;

	void main () {
		gl_Position =		pos_clip;

		vs_uv =		        uv;
		vs_tex_indx =		tex_indx;
		vs_brightness =		brightness;
	}
$endif

$if fragment
	in vec2	    vs_uv;
	in float	vs_tex_indx;
	in float	vs_brightness;

	uniform	sampler2DArray tile_textures;

	uniform bool alpha_test;
	#define ALPHA_TEST_THRES 127.0

	out vec4 frag_col;

	void main () {
		vec4 col = texture(tile_textures, vec3(vs_uv, vs_tex_indx));
		col.rgb *= vs_brightness;

		if (alpha_test) {
			if (col.a <= ALPHA_TEST_THRES / 255.0)
				discard;
			col.a = 1.0;
		}

		frag_col = col;
	}
$endif
