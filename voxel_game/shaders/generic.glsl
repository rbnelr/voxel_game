#version 330 core

#define ALLOW_WIREFRAME 1
$include "common.glsl"

$if vertex
	layout (location = 0) in vec3	pos_model;
	layout (location = 1) in vec4	color;

	uniform mat4 model_to_world;

	out vec4	vs_color;

	void main () {
		gl_Position =		cam_to_clip * world_to_cam * model_to_world * vec4(pos_model, 1);
		vs_color =			color;

		WIREFRAME_MACRO;
	}
$endif

$if fragment
	in vec4		vs_color;

	void main () {
		FRAG_COL(vs_color);
	}
$endif