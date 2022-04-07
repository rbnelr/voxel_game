#version 460 core
#include "fullscreen_triangle.glsl"

#ifdef _FRAGMENT
	uniform sampler2D input_tex;
	
	uniform float gauss_radius_px = 20;
	
	uniform float exposure = 1.0;
	
	float gauss_kernel (float x) {
		float sigma = 0.35 * gauss_radius_px;
		
		return exp((x*x) / (-2.0 * sigma * sigma));
	}
	
	out vec3 frag_col;
	void main () {
		vec2 px_sz = 1.0 / textureSize(input_tex, 0).xy;
		
		vec3 col = vec3(0.0);
		float total = 0.0;
		
		int r = int(ceil(gauss_radius_px));
		
	#if PASS == 0
		for (int i=-r; i<=r; ++i) {
			float weight = gauss_kernel(float(i));
			
			col   += weight * texture(input_tex, vs_uv + vec2(px_sz.x * float(i), 0.0)).rgb;
			total += weight;
		}
		col /= total;
	#else
		for (int i=-r; i<=r; ++i) {
			float weight = gauss_kernel(float(i));
			
			col   += weight * texture(input_tex, vs_uv + vec2(0.0, px_sz.y * float(i))).rgb;
			total += weight;
		}
		col /= total;
		col *= exposure;
	#endif
		
		frag_col = col;
	}
#endif
