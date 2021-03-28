#version 460 core
#include "fullscreen_triangle.glsl"

#ifdef _FRAGMENT
	uniform sampler2D main_color;
	uniform sampler2D bloom;

	uniform bool enable_bloom;
	uniform float exposure = 1.0;
	
	float haarm_peter_duiker (float col) {
		float x = max(float(0), col - float(0.004));
		float c = (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
		return pow(c, float(2.2));
	}
	
	// All components are in the range [0…1], including hue.
	vec3 rgb2hsv (vec3 c) {
		vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
		vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
		vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

		float d = q.x - min(q.w, q.y);
		float e = 1.0e-10;
		return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
	}
	
	// All components are in the range [0…1], including hue.
	vec3 hsv2rgb (vec3 c) {
		vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
		vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
		return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
	}
	
	out vec3 frag_col;
	void main () {
		vec3 col = texture(main_color, vs_uv).rgb;
		col *= exposure;
		
		if (enable_bloom) {
			vec3 bloom = texture(bloom, vs_uv).rgb;
			
			//vec3 hsv = rgb2hsv(bloom);
			
			bloom = bloom / (bloom + vec3(0.9));
			
			//float exposure = 1.0;
			//bloom = vec3(1.0) - exp(-bloom * vec3(exposure));
			
			//bloom = hsv2rgb(hsv);
			
			col += bloom;
		}

		// could tonemap here
		
		//float exposure = 1.0;
		//col = vec3(1.0) - exp(-col * vec3(exposure));
		
		//col = col / (col + vec3(1.0));
		
		//float m = max(max(col.r, col.g), col.b);
		//if (m > 0.9) {
		//	float f = clamp((m - 0.9) * 0.2, 0.0, 1.0);
		//	
		//	col = mix(col, vec3(1.0), vec3(f));
		//}
		
		vec3 hsv = rgb2hsv(col);
		//hsv.z = hsv.z / (hsv.z + 1.0);
		
		//hsv.z = haarm_peter_duiker(hsv.z);
		
		col = hsv2rgb(hsv);
		
		frag_col = col;
	}
#endif
