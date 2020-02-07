#version 330 core

#define ALLOW_WIREFRAME 1
$include "common.glsl"

$if vertex
	layout (location = 0) in vec3	pos_model;
	layout (location = 1) in float	brightness;
	layout (location = 2) in vec2	uv;
	layout (location = 3) in float	tex_indx;
	layout (location = 4) in float	hp_ratio;

	uniform vec3 chunk_pos;

	//out vec3	vs_pos_cam;
	out float	vs_brightness;
	out vec2	vs_uv;
	out float	vs_tex_indx;
	out float	vs_hp_ratio;

	void main () {
		gl_Position =		world_to_clip * vec4(pos_model + chunk_pos, 1);

		//vs_pos_cam =		pos_cam.xyz;
		vs_brightness =		brightness;
		vs_uv =		        uv;
		vs_tex_indx =		tex_indx;
		vs_hp_ratio =		hp_ratio;

		WIREFRAME_MACRO;
	}
$endif

$if fragment
	//in vec3		vs_pos_cam;
	in float	vs_brightness;
	in vec2	    vs_uv;
	in float	vs_tex_indx;
	in float	vs_hp_ratio;

	uniform	sampler2DArray tile_textures;
	uniform	sampler2DArray breaking_textures;

	uniform float breaking_frames_count;
	uniform float breaking_mutliplier;

	uniform bool alpha_test;
	#define ALPHA_TEST_THRES 127.0

	void main () {
		vec4 col = texture(tile_textures, vec3(vs_uv, vs_tex_indx));
	
		if (vs_hp_ratio < 1) {
			float breaking_frame = floor(vs_hp_ratio * breaking_frames_count + 0.00001f);
		
			if (breaking_frame < breaking_frames_count) {
				col.rgb *= 1 + (texture(breaking_textures, vec3(vs_uv, breaking_frame)).rrr * 255.0 - 127.0) / 127.0 * breaking_mutliplier;
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
