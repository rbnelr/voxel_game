#version 330 core

#define ALLOW_WIREFRAME 1
$include "common.glsl"

$if vertex
	layout (location = 0) in vec3	pos_model;
	layout (location = 1) in float	brightness;
	layout (location = 2) in vec4	uv_indx_hp;
	layout (location = 3) in vec4	color;

	uniform vec3 chunk_pos;

	out vec3	vs_pos_cam;
	out float	vs_brightness;
	out vec4	vs_uv_indx_hp;
	out vec4	vs_color;

	void main () {
		vec3 pos_cam =		(world_to_cam * vec4(pos_model + chunk_pos, 1)).xyz;

		gl_Position =		cam_to_clip * vec4(pos_cam, 1);

		vs_pos_cam =		pos_cam;
		vs_brightness =		brightness;
		vs_uv_indx_hp =		uv_indx_hp;
		vs_color =			color;

		WIREFRAME_MACRO;
	}
$endif

$if fragment
	in vec3		vs_pos_cam;
	in float	vs_brightness;
	in vec4		vs_uv_indx_hp;
	in vec4		vs_color;

	uniform	sampler2DArray tile_textures;
	uniform	sampler2DArray breaking_textures;

	uniform float breaking_frames_count;
	uniform float breaking_mutliplier;

	uniform bool alpha_test;
	#define ALPHA_TEST_THRES 127.0

	void main () {
		float hp = vs_uv_indx_hp.w;

		vec4 col = texture(tile_textures, vs_uv_indx_hp.xyz);
	
		if (hp < 1) {
			float breaking_frame = floor(hp * breaking_frames_count + 0.00001f);
		
			if (breaking_frame < breaking_frames_count) {
				col.rgb *= 1 + (texture(breaking_textures, vec3(vs_uv_indx_hp.xy, breaking_frame)).rrr * 255.0 - 127.0) / 127.0 * breaking_mutliplier;
			}
		}
	
		col.rgb *= vec3(vs_brightness);
		
		if (alpha_test) {
			if (col.a <= ALPHA_TEST_THRES / 255.0)
				DISCARD();
			col.a = 1.0;
		}

		FRAG_COL(col);
	}
$endif
