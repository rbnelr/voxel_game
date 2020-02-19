#version 330 core

#define ALLOW_WIREFRAME 1
$include "common.glsl"
$include "fog.glsl"

$if vertex
	layout (location = 0) in vec3	pos_model;
	layout (location = 1) in vec2	uv;
	layout (location = 2) in int	tex_indx;
	layout (location = 3) in int	light_level;
	layout (location = 4) in int	hp;

	uniform vec3 chunk_pos;

	out vec3	vs_pos_cam;
	out vec2	vs_uv;
	out float	vs_tex_indx;
	out float	vs_brightness;
	out float	vs_hp_ratio;

	void main () {
		vec4 pos_cam = world_to_cam * vec4(pos_model + chunk_pos, 1);
		
		gl_Position =		cam_to_clip * pos_cam;

		vs_pos_cam =		pos_cam.xyz;
		vs_uv =		        uv;
		vs_tex_indx =		float(tex_indx);
		vs_brightness =		float(light_level) / 15.0 * 0.96 + 0.04;
		vs_hp_ratio =		float(hp) / 255.0;

		WIREFRAME_MACRO;
	}
$endif

$if fragment
	in vec3		vs_pos_cam;
	in vec2	    vs_uv;
	in float	vs_tex_indx;
	in float	vs_brightness;
	in float	vs_hp_ratio;

	uniform	sampler2DArray tile_textures;
	uniform	sampler2DArray breaking_textures;

	uniform float breaking_frames_count;
	uniform float breaking_mutliplier;

	uniform bool alpha_test;
	#define ALPHA_TEST_THRES 127.0

	void main () {
		float dist_sqr = dot(vs_pos_cam, vs_pos_cam);
		vec3 dir_world = (cam_to_world * vec4(vs_pos_cam, 0)).xyz / sqrt(dist_sqr);
		
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

		col.rgb = apply_fog(col.rgb, dist_sqr, dir_world);
		FRAG_COL(col);
	}
$endif
