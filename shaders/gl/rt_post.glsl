#version 460 core
#include "fullscreen_triangle.glsl"

#ifdef _FRAGMENT
	#include "rt_util.glsl"
	
	uniform sampler2D framebuf_col;
	
	uniform ivec2 framebuf_size;
	
	uniform float gauss_radius_px = 20;
	
	uniform float exposure = 1.0;
	
	float gauss_kernel (vec2 offs) {
		float sigma = 0.35 * gauss_radius_px;
		
		float a = offs.x*offs.x + offs.y*offs.y;
		float b = -2.0 * sigma * sigma;
		return exp(a / b);
	}
	
	out vec3 frag_col;
	void main () {
		vec2 px_sz = 1.0 / vec2(framebuf_size);
		
		vec3 col = vec3(0.0);
		float total = 0.0;
		
		int r = int(ceil(gauss_radius_px));
		for (int x=-r; x<=r; ++x) {
			for (int y=-r; y<=r; ++y) {
				vec2 offs = vec2(ivec2(x,y));
				float weight = gauss_kernel(offs);
				
				col += texture(framebuf_col, vs_uv + px_sz * offs).rgb * weight;
				total += weight;
			}
		}
		col /= total;
		
		frag_col = col * exposure;
	}
#endif
