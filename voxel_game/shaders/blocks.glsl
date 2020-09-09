#version 330 core

#define ALLOW_WIREFRAME 1
$include "common.glsl"
$include "fog.glsl"

$if vertex
	layout (location = 0) in vec3	pos_model;
	layout (location = 1) in vec2	uv;
	layout (location = 2) in float	tex_indx;

	uniform vec3 chunk_pos;
	uniform float sky_light_reduce;

	out vec3	vs_pos_cam;
	out vec2	vs_uv;
	out float	vs_tex_indx;

	void main () {
		vec4 pos_cam = world_to_cam * vec4(pos_model + chunk_pos, 1);
		
		gl_Position =		cam_to_clip * pos_cam;

		vs_pos_cam =		pos_cam.xyz;
		vs_uv =		        uv;
		vs_tex_indx =		tex_indx;

		WIREFRAME_MACRO;
	}
$endif

$if fragment
	in vec3		vs_pos_cam;
	in vec2	    vs_uv;
	in float	vs_tex_indx;

	uniform	sampler2DArray tile_textures;

	#define ALPHA_TEST_THRES 127.0

	void main () {
		float dist_sqr = dot(vs_pos_cam, vs_pos_cam);
		vec3 dir_world = (cam_to_world * vec4(vs_pos_cam, 0)).xyz / sqrt(dist_sqr);
		
		vec4 col = texture(tile_textures, vec3(vs_uv, vs_tex_indx));
	
		//col.rgb *= vec3(vs_brightness);
		
		if (col.a <= ALPHA_TEST_THRES / 255.0)
			DISCARD();
		col.a = 1.0;

		col.rgb = apply_fog(col.rgb, dist_sqr, dir_world);
		FRAG_COL(col);
	}
$endif
