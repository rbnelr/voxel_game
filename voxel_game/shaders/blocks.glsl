#version 330 core

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

	const float[] LUT = float[5] (
			0.02,
			0.08,
			0.3,
			0.6,
			1
		);

	void main () {
		vec3 pos_cam =		(world_to_cam * vec4(pos_model + chunk_pos, 1)).xyz;

		gl_Position =		cam_to_clip * vec4(pos_cam, 1);

		vs_pos_cam =		pos_cam;
		vs_brightness =		LUT[ int(brightness) ];
		vs_uv_indx_hp =		uv_indx_hp;
		vs_color =			color;
	}
$endif

$if fragment
	in vec3		vs_pos_cam;
	in float	vs_brightness;
	in vec4		vs_uv_indx_hp;
	in vec4		vs_color;

	uniform	sampler2D	test;

	uniform int texture_res;
	uniform int atlas_textures_count;
	uniform int breaking_frames_count;
	uniform bool show_dbg_tint;

	uniform bool alpha_test;

	out vec4 frag_col;

	void main () {
		
		vec2 face_uv = vs_uv_indx_hp.xy;
	
		//uv.x /= 3;
		//uv.x += vs_uvzw_atlas.z / 3;
		//
		//uv.y /= atlas_textures_count;
		//uv.y += vs_uvzw_atlas.w / atlas_textures_count;
	
		vec4 col = texture(test, face_uv / vec2(1,3)).rgba;
	
		//{
		//	int breaking_frame = int(floor(vs_hp_ratio * float(breaking_frames_count) +0.1f));
		//
		//	if (breaking_frame < 10) {
		//		vec2 breaking_uv = face_uv;
		//	
		//		breaking_uv.y /= float(breaking_frames_count);
		//		breaking_uv.y += float(breaking_frame) / float(breaking_frames_count);
		//	
		//		col.rgb *= (texture(breaking, breaking_uv).rgb * 255 -127) / 127 +1;
		//	}
		//}
	
		col.rgb *= vec3(vs_brightness);
		
		//if (alpha_test) {
		//	if (col.a <= 110.0/255) discard;
		//	col.a = 1;
		//}
	
		frag_col = col;
	}
$endif
