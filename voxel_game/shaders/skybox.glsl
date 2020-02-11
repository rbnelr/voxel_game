#version 330 core

$include "common.glsl"
$include "fog.glsl"

$if vertex
	layout (location = 0) in vec3 world_dir;
	
	out		vec3	vs_world_dir;

	void main () {
		gl_Position =	cam_to_clip * vec4(mat3(world_to_cam) * world_dir, 1);
		vs_world_dir =	world_dir;
	}
$endif

$if fragment
	in		vec3	vs_world_dir;

	vec3 srgb (float r, float g, float b) {
		return pow( vec3(r,g,b) / 255.0, vec3(2.2) );
	}

	out		vec4	frag_color;

	void main () {
		frag_color = vec4(fog_color(normalize(vs_world_dir)), 1);
	}
$endif
