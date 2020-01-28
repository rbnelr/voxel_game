#version 150 core // version 3.2

in		vec3	vs_pos_cam;
in		vec4	vs_color;

void main () {
	gl_FragColor = vs_color;
}
