#version 330 core

#define ALLOW_WIREFRAME 1
$include "common.glsl"

$if vertex
	layout (location = 0) in vec3	pos_model;
	layout (location = 1) in vec3	normal;
	layout (location = 2) in vec2	uv;
	layout (location = 3) in float	tex_indx;

	uniform mat4 model_to_world;

	out vec3	vs_normal_world;
	out vec2	vs_uv;
	out float	vs_tex_indx;

	void main () {
		gl_Position =		cam_to_clip * world_to_cam * model_to_world * vec4(pos_model, 1);

		vs_normal_world =	(model_to_world * vec4(normal, 0)).xyz;
		vs_uv =				uv;
		vs_tex_indx =		tex_indx;

		WIREFRAME_MACRO;
	}
$endif

$if fragment
	in vec3		vs_normal_world;
	in vec2		vs_uv;
	in float	vs_tex_indx;
	
	uniform	sampler2DArray tile_textures;

	const vec3 light_dir_world = normalize(vec3(0.05, 0.5, 3));

	void main () {
		vec4 col = texture(tile_textures, vec3(vs_uv, vs_tex_indx));
		col.rgb *= max(dot(normalize(vs_normal_world), light_dir_world), 0.0) * 0.7 + 0.3;
		
		FRAG_COL(col);
	}
$endif
