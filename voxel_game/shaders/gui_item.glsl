#version 330 core

$include "common.glsl"

$if vertex
	layout (location = 0) in vec2 pos_px;
	layout (location = 1) in vec2 uv;
	layout (location = 2) in float tex_indx;

	out vec2 vs_uv;
	out float vs_tex_indx;

	void main () {
		gl_Position = vec4(pos_px / viewport_size * 2 - 1, 0, 1);

		vs_uv = uv;
		vs_tex_indx = tex_indx;
	}
$endif

$if fragment
	in vec2	vs_uv;
	in float vs_tex_indx;

	uniform	sampler2DArray tile_textures;

	uniform	sampler2DArray tile_textures2;
	uniform	float blah;

	void main () {
		frag_col = texture(tile_textures, vec3(vs_uv, vs_tex_indx));
	}
$endif
