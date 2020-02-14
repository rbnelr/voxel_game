#version 330 core

$include "common.glsl"

$if vertex
	layout (location = 0) in vec2 pos_px;
	layout (location = 1) in vec2 uv;
	layout (location = 2) in vec4 col;

	out vec2 vs_uv;
	out vec4 vs_col;

	void main () {
		gl_Position = vec4(pos_px / viewport_size * 2 - 1, 0, 1);

		vs_uv = uv;
		vs_col = col;
	}
$endif

$if fragment
	in vec2	vs_uv;
	in vec4	vs_col;

	out vec4 frag_col;

	uniform	sampler2D tex;

	void main () {
		frag_col = mix(texture(tex, vs_uv) * vs_col, vs_col, step(vs_uv.x, 0));
	}
$endif
