#version 150 core // version 3.2

$include "_common.vert"

in		vec3	pos_world;
in		vec4	uvzw_atlas;
in		float	hp_ratio;
in		float	brightness;
in		vec4	dbg_tint;

out		vec3	vs_pos_cam;
out		vec4	vs_uvzw_atlas;
out		float	vs_hp_ratio;
out		float	vs_brightness;
out		vec4	vs_dbg_tint;

uniform	mat4	world_to_cam;
uniform	mat4	cam_to_clip;

const float[] LUT = float[5] (
	0.02,
	0.08,
	0.3,
	0.6,
	1
);

void main () {
	vec3 pos_cam =		(world_to_cam * vec4(pos_world,1)).xyz;
	
	gl_Position =		cam_to_clip * vec4(pos_cam, 1);
	
	vs_pos_cam =		pos_cam;
	vs_uvzw_atlas =		uvzw_atlas;
	vs_hp_ratio =		hp_ratio;
	vs_brightness =		LUT[ int(brightness) ];
	vs_dbg_tint =		dbg_tint;
	
	WIREFRAME_MACRO;
}
