#version 330 core

$include "common.glsl"

$if vertex
	layout (location = 0) in vec3 pos_world;
	layout (location = 1) in int mode; // fill_mode
	layout (location = 2) in vec4 color;

	out		vec3	vs_pos_cam;
	flat out int	vs_mode;
	out		vec4	vs_color;

	void main () {
		vec3 pos_cam =		(world_to_cam * vec4(pos_world,1)).xyz;
	
		gl_Position =		cam_to_clip * vec4(pos_cam, 1);
	
		vs_pos_cam =		pos_cam;
		vs_mode =			mode;
		vs_color =			color;
	}
$endif

$if fragment
	in		vec3	vs_pos_cam;
	flat in	int		vs_mode;
	in		vec4	vs_color;

	void main () {
		//vec2 uv = gl_FragCoord.xy;
		//int a = int(floor(dot(uv, vec2(0.8, 1.12355)) / 12));
		//int b = int(floor(dot(uv, vec2(-0.48, 1.0988656)) / 12));
		//
		//if (vs_mode == 0) {
		//	// fill
		//} else {
		//	// striped
		//	if ((a & 1) != 0 || (b & 1) != 0) {
		//		discard;
		//	}
		//}

		frag_col = vs_color;
	}
$endif
