#version 330 core

$include "common.glsl"
$include "fog.glsl"

$if vertex
	const vec4[] pos_clip = vec4[] (
		vec4(+1,-1, 0, 1),
		vec4(+1,+1, 0, 1),
		vec4(-1,-1, 0, 1),
		vec4(-1,-1, 0, 1),
		vec4(+1,+1, 0, 1),
		vec4(-1,+1, 0, 1)
	);
	const vec2[] uv = vec2[] (
		vec2(1,0),
		vec2(1,1),
		vec2(0,0),
		vec2(0,0),
		vec2(1,1),
		vec2(0,1)
	);

	out vec2 vs_uv;

	void main () {
		gl_Position = pos_clip[gl_VertexID];
		vs_uv       = uv[gl_VertexID];
	}
$endif

$if fragment
	in vec2 vs_uv;

	uniform float slider;
	uniform sampler2D rendertexture;
	
	void main () {
		vec2 ndc = gl_FragCoord.xy / viewport_size * 2.0 - 1.0;
		if (ndc.x > (slider * 2 - 1))
			DISCARD();

		vec4 col = texture(rendertexture, vs_uv);

		FRAG_COL(col);
	}
$endif
