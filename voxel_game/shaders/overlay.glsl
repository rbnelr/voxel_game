#version 330 core

$include "common.glsl"

$if vertex
	layout (location = 0) in vec3 pos_world;
	layout (location = 1) in vec4 color;

	out		vec3	vs_pos_cam;
	out		vec4	vs_color;

	void main () {
		vec3 pos_cam =		(world_to_cam * vec4(pos_world,1)).xyz;
	
		gl_Position =		cam_to_clip * vec4(pos_cam, 1);
	
		vs_pos_cam =		pos_cam;
		vs_color =			color;
	}
$endif

$if fragment
	in		vec3	vs_pos_cam;
	in		vec4	vs_color;

	void main () {
		frag_col = vs_color;
	}
$endif
