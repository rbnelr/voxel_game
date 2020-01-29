#version 150 core // version 3.2

in		vec3	vs_pos_cam;
in		vec4	vs_color;

out		vec4	frag_col;

void main () {
	frag_col = vs_color;
}
