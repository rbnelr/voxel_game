#version 330 core

$if vertex
	layout (location = 0) in vec3 world_dir;
	
	out		vec3	vs_world_dir;

	uniform	mat4	world_to_cam;
	uniform	mat4	cam_to_clip;

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
		vec3 dir = normalize(vs_world_dir);
	
		vec3 sky_col =		srgb(190,239,255);
		vec3 horiz_col =	srgb(204,227,235);
		vec3 down_col =		srgb(41,49,52);
	
		vec3 col;
		if (dir.z > 0)
			col = mix(horiz_col, sky_col, dir.z);
		else
			col = mix(horiz_col, down_col, -dir.z);
	
		frag_color = vec4(col, 1);
	}
$endif
