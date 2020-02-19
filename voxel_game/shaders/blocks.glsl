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

	/* generate.py
		amount = 1.0
		values = []
		for i in range(0,16):
		  values = [amount] + values
		  amount = amount * 0.8

		for v in values:
		  print('{:.4f}'.format(v), end=', ')
	*/
	const float[] light_level_LUT = float[16] (
		0.0352, 0.0440, 0.0550, 0.0687, 0.0859, 0.1074, 0.1342, 0.1678, 0.2097, 0.2621, 0.3277, 0.4096, 0.5120, 0.6400, 0.8000, 1.0000
	);

	void main () {
		vec4 pos_cam = world_to_cam * vec4(pos_model + chunk_pos, 1);
		
		gl_Position =		cam_to_clip * pos_cam;

		vs_pos_cam =		pos_cam.xyz;
		vs_uv =		        uv;
		vs_tex_indx =		float(tex_indx);
		vs_brightness =		light_level_LUT[light_level];
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
