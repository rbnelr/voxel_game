#version 330 core

#define ALLOW_WIREFRAME 1
$include "common.glsl"

$if vertex
	layout (location = 0) in vec3	pos_model;
	layout (location = 1) in vec3	normal;
	layout (location = 2) in vec4	color;

	uniform mat4 model_to_world;

	out vec3	vs_normal_world;
	out vec4	vs_color;

	void main () {
		gl_Position =		cam_to_clip * world_to_cam * model_to_world * vec4(pos_model, 1);

		vs_normal_world =	(model_to_world * vec4(normal, 0)).xyz;
		vs_color =			color;

		WIREFRAME_MACRO;
	}
$endif

$if fragment
	in vec4		vs_color;
	in vec3		vs_normal_world;

	const vec3 light_dir_world = normalize(vec3(0.05, 0.5, 3));

	void main () {
		FRAG_COL(vs_color * max(dot(normalize(vs_normal_world), light_dir_world), 0.0) * 0.7 + 0.3);
	}
$endif
