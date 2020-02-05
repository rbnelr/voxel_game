#version 330 core

$if vertex
	layout (location = 0) in vec4 pos_clip;
	layout (location = 1) in vec2 uv;

	out vec2 vs_uv;

	void main () {
		gl_Position = pos_clip;

		vs_uv = uv;
	}
$endif

$if fragment
	in vec2	vs_uv;

	out vec4 frag_col;

	uniform	sampler2D tex;

	void main () {
		frag_col = texture(tex, vs_uv);
	}
$endif
