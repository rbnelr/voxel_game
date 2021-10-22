#version 460 core
#include "fullscreen_triangle.glsl"

#ifdef _FRAGMENT
	//uniform sampler2D main_color;
	uniform float exposure = 1.0;
	
	out vec3 frag_col;
	void main () {
		//vec3 col = texture(main_color, vs_uv).rgb;
		//col *= exposure;
		
		frag_col = vec3(1,0,0);
	}
#endif
