#version 150 core // version 3.2

$if vertex
	in		vec3	pos_model;
	in		vec4	color;

	out		vec3	vs_pos_cam;
	out		vec4	vs_color;

	uniform	mat4	world_to_cam;
	uniform	mat4	cam_to_clip;

	uniform	vec3	block_pos;
	uniform	mat3	face_rotation;

	void main () {
		vec3 pos_cam =		(world_to_cam * vec4(face_rotation * pos_model + block_pos + vec3(0.5), 1)).xyz;
	
		gl_Position =		cam_to_clip * vec4(pos_cam, 1);
	
		vs_pos_cam =		pos_cam;
		vs_color =			color;
	}
$endif

$if fragment
	in		vec3	vs_pos_cam;
	in		vec4	vs_color;

	out		vec4	frag_col;

	void main () {
		frag_col = vs_color;
	}
$endif
